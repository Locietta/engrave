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

#include <vector>

#include "draw/types/font.h"
#include "engravingitem.h"

namespace muse::io {
struct PathT;
} // namespace muse::io

namespace mu::engraving {
struct Chord;
struct Factory;
struct Harmony;
struct StringData;
struct Transaction;

// Keep this in order - not used directly for comparisons, but the dots will appear in
// this order in fret multidot mode. See fretproperties.cpp.
enum class FretDotType : signed char { NORMAL = 0, CROSS, SQUARE, TRIANGLE = 3 };

enum class FretMarkerType : signed char { NONE, CIRCLE, CROSS };

struct FretItem {
    struct Barre {
        int start_string = -1;
        int end_string = -1;

        Barre() = default;

        Barre(int s, int e) : start_string(s), end_string(e) {}

        bool exists() const { return start_string > -1; }
    };

    struct Dot {
        int fret = 0;
        FretDotType dtype = FretDotType::NORMAL;
        int fingering = 0; // NOTE:JT - possible future feature?

        Dot() = default;

        Dot(int f, FretDotType t = FretDotType::NORMAL, bool is_part_of_slur_barre = false)
            : fret(f), dtype(t), is_part_of_slur_barre(is_part_of_slur_barre) {}

        bool exists() const { return fret > 0; }

        bool is_part_of_slur_barre = false;
    };

    struct Marker {
        FretMarkerType mtype = FretMarkerType::NONE;

        Marker() = default;

        Marker(FretMarkerType t) : mtype(t) {}

        bool exists() const { return mtype != FretMarkerType::NONE; }
    };

    struct MarkerTypeNameItem {
        FretMarkerType mtype = FretMarkerType::NONE;
        const char *name = nullptr;
    };

    struct DotTypeNameItem {
        FretDotType dtype = FretDotType::NORMAL;
        const char *name = nullptr;
    };

    static const std::vector<FretItem::MarkerTypeNameItem> marker_type_name_map;
    static const std::vector<FretItem::DotTypeNameItem> dot_type_name_map;

    static Char marker_to_char(FretMarkerType t);
    static String marker_type_to_name(FretMarkerType t);
    static FretMarkerType name_to_marker_type(String n);
    static String dot_type_to_name(FretDotType t);
    static FretDotType name_to_dot_type(String n);
};

// The three main storage containers used by fret diagrams
typedef std::map<int, FretItem::Barre> BarreMap;
typedef std::map<int, FretItem::Marker> MarkerMap;
typedef std::map<int, std::vector<FretItem::Dot>> DotMap;

struct DiagramInfo {
    String harmony_name;
    String diagram_xml;
    String diagram_pattern;
};

//---------------------------------------------------------
//   @@ FretDiagram
///    Fretboard diagram
//
//   @P userMag    double
//   @P strings    int  number of strings
//   @P frets      int  number of frets
//   @P fretOffset int
//
//   Note that, while strings are zero-indexed, frets are one-indexed
//---------------------------------------------------------

struct FretDiagram final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::FRET_DIAGRAM)

public:

    ~FretDiagram();

    EngravingItem *linked_clone() override;

    FretDiagram *clone() const override { return new FretDiagram(*this); }

    Segment *segment() const;

    String pattern_from_diagram() const;
    std::vector<String> harmonies_from_pattern(const String &pattern) const;
    std::vector<DiagramInfo> patterns_from_harmony(const String &harmony_name);

    void update_diagram(const String &harmony_name);

    double main_width() const;

    int strings() const { return strings_; }

    int frets() const { return frets_; }

    void set_strings(int n);

    void set_frets(int n) { frets_ = n; }

    void set_dot(int string, int fret, bool add = false, FretDotType dtype = FretDotType::NORMAL);
    void add_dot_for_dot_style_barre(int string, int fret);
    void remove_dot_for_dot_style_barre(int string, int fret);
    void set_barre(int start_string, int end_string, int fret);
    void set_barre(int string, int fret, bool add = false);
    void set_marker(int string, FretMarkerType marker);
    /*void setFingering(int string, int finger);*/

    void clear();
    bool is_clear() const;

    void undo_set_fret_dot(int input_string, int input_fret, bool input_add = false, FretDotType input_dtype = FretDotType::NORMAL);
    void undo_set_fret_marker(int input_string, FretMarkerType input_mtype);
    void undo_set_fret_barre(int input_string, int input_fret, bool input_add = false);
    void undo_fret_clear();

    int fret_offset() const { return fret_offset_; }

    void set_fret_offset(int val) { fret_offset_ = val; }

    int max_frets() const { return max_frets_; }

    void set_max_frets(int val) { max_frets_ = val; }

    bool show_nut() const { return show_nut_; }

    void set_show_nut(bool val) { show_nut_ = val; }

    double user_mag() const { return user_mag_; }

    void set_user_mag(double m) { user_mag_ = m; }

    int num_pos() const;

    Orientation orientation() const { return orientation_; }

    String harmony_plain_text() const;
    String harmony_display_text() const;

    Harmony *harmony() const { return harmony_; }

    void set_harmony(String harmony_text);

    std::vector<FretItem::Dot> dot(int s, int f = 0) const;
    FretItem::Marker marker(int s) const;
    FretItem::Barre barre(int fret) const;

    const BarreMap &barres() const { return barres_; }

    const DotMap &dots() const { return dots_; }

    const MarkerMap &markers() const { return markers_; }

    muse::draw::Font fret_num_font() const;
    muse::draw::Font fingering_font() const;

    void init(StringData *, Chord *);
    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    void set_track(track_idx_t val) override;

    bool show_fingering() const { return show_fingering_; }

    void set_show_fingering(bool v) { show_fingering_ = v; }

    const std::vector<int> &fingering() const { return fingering_; }

    void set_fingering(std::vector<int> v);

    static FretDiagram *make_from_harmony_or_fret_diagram(const EngravingItem *harmony_or_fret_diagram);

    bool is_in_fret_box() const;
    bool is_custom(const String &harmony_name_for_compare) const;

    bool allow_time_anchor() const override { return ownership_parent() && parent()->is_segment(); }

    friend struct FretUndoData;

    struct FingeringItem {
        String finger_number;
        PointF pos;

        FingeringItem(String s, PointF p) : finger_number(s), pos(p) {}
    };

    struct LayoutData : public EngravingItem::LayoutData {
        double string_line_width = 0.0;
        double nut_line_width = 0.0;
        double nut_y = 0.0;
        double string_dist = 0.0;
        double fret_dist = 0.0;
        double marker_size = 0.0;
        double marker_y = 0.0;
        double string_extend_top = 0.0;
        double string_extend_bottom = 0.0;
        double dot_diameter = 0.0;
        double fret_num_padding = 0.0;
        double grid_height = 0.0;
        std::vector<FingeringItem> fingering_items;
        std::vector<PainterPath> slur_paths;
        String fret_text = String();
    };
    DECLARE_LAYOUTDATA_METHODS(FretDiagram)

private:
    friend struct Factory;

    FretDiagram(DummyParentOr<Segment> parent);
    FretDiagram(const FretDiagram &);

    void read_harmony_to_diagram_file(const muse::io::PathT &file_path) const;

    void init_default_values();

    void remove_dot(int s, int f = 0);
    void remove_barre(int f);
    void remove_barres(int string, int fret = 0);
    void remove_marker(int s);
    void remove_dots_markers(int ss, int es, int fret);

    static void apply_diagram_pattern(FretDiagram *diagram, const String &pattern);

    int strings_ = 0;
    int frets_ = 0;
    int fret_offset_ = 0;
    int max_frets_ = 0;
    bool show_nut_ = false;
    Orientation orientation_ = Orientation::VERTICAL;

    // Barres are stored in the format: K: fret, V: barre struct
    BarreMap barres_;

    // Dots stored as K: string, V: dot struct
    DotMap dots_;

    // Markers stored as K: string, V: marker struct
    MarkerMap markers_;

    Harmony *harmony_ = nullptr;

    double user_mag_ = 1.0; // allowed 0.1 - 10.0

    bool show_fingering_ = false;
    std::vector<int> fingering_ = std::vector<int>(strings_, 0);
};
} // namespace mu::engraving
