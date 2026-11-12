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

#include "engravingitem.h"
#include "line.h"
#include "property.h"
#include "textbase.h"

namespace mu::engraving {
enum class GuitarBendType : unsigned char {
    BEND,
    PRE_BEND,
    GRACE_NOTE_BEND,
    SLIGHT_BEND,

    DIVE,
    PRE_DIVE,
    DIP,
    SCOOP,
};

enum class GuitarBendShowHoldLine : unsigned char {
    AUTO,
    SHOW,
    HIDE,
};

enum class ActionIconType : signed char;

struct GuitarBend final : SLine {
private:
    DECLARE_CLASSOF(ElementType::GUITAR_BEND)

    M_PROPERTY2(DirectionV, direction, set_direction, DirectionV::AUTO)
    M_PROPERTY2(int, bend_amount_in_quarter_tones, set_bend_amount_in_quarter_tones, 4)
    M_PROPERTY2(GuitarBendShowHoldLine, show_hold_line, set_show_hold_line, GuitarBendShowHoldLine::AUTO)
    M_PROPERTY2(float, start_time_factor, set_start_time_factor, 0.f)
    M_PROPERTY2(float, end_time_factor, set_end_time_factor, 1.f)

public:
    GuitarBend(EngravingObject *parent);
    GuitarBend(const GuitarBend &);
    ~GuitarBend() override;

    GuitarBend *clone() const override { return new GuitarBend(*this); }

    LineSegment *create_line_segment() override;

    Anchor anchor() const override { return Anchor::NOTE; }

    bool allow_time_anchor() const override { return false; }

    Note *start_note() const;
    Note *start_note_of_chain() const;

    Note *end_note() const;
    void change_bend_amount(int bend_amount, int start_bend_amount);
    void set_end_note_pitch(int pitch, int quarter_tone_offset);

    bool is_release_bend() const;
    bool is_full_release() const;
    bool angled_pre_bend() const;
    bool is_dive() const;
    bool is_full_release_dive() const;

    static void fix_notes_fretting_for_grace_bend(Note *grace, Note *main);
    static void fix_notes_fretting_for_standard_bend(Note *start_note, Note *end_note);
    static Note *create_end_note(Note *start_note, GuitarBendType bend_type = GuitarBendType::BEND);

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;

    static constexpr int slack_bend_amount = -33;
    // Semantic normalization suppresses the display text/geometry refresh.
    void compute_bend_amount(bool display = true);
    int tot_bend_amount_including_preceding_bends() const;
    void compute_bend_text();
    void compute_is_invalid_or_needs_warning();

    bool is_invalid() const { return is_invalid_; }

    bool is_borderline_unplayable() const { return is_borderline_unplayable_; }

    GuitarBend *overlapping_bend_or_dive() const;

    GuitarBend *find_preceding_bend() const;
    GuitarBend *find_following_pre_bend_or_dive() const;
    WhammyBar *find_overlapping_whammy_bar(Fraction start_tick, Fraction end_tick) const;

    void update_hold_line();

    void set_hold_line(GuitarBendHold *hold) { hold_line_ = hold; }

    GuitarBendHold *hold_line() const { return hold_line_; }

    double line_width() const;

    Color cur_color(const rendering::PaintOptions &opt) const override;

    DirectionV dive_tab_pos() const { return dive_tab_pos_; }

    void set_dive_tab_pos(DirectionV v) { dive_tab_pos_ = v; }

    static void adapt_bends_from_tab_to_standard_staff(const Staff *staff);

    static GuitarBendType bend_type_from_action_icon(ActionIconType action_icon_type);

    GuitarBendType bend_type() const { return bend_type_; }

    void set_bend_type(GuitarBendType t);

    VibratoType dip_vibrato_type() const { return dip_vibrato_type_; }

    void set_dip_vibrato_type(VibratoType v) { dip_vibrato_type_ = v; }

    bool is_slack() const { return is_slack_; }

    void set_is_slack(bool v) { is_slack_ = v; }

    std::optional<float> target_time_factor() const { return target_time_factor_; }

    void set_target_time_factor(float v);

    struct LayoutData : public SLine::LayoutData {
    public:
        bool up() const { return up_; }

        void set_up(bool v) { up_ = v; }

        bool is_inside() const { return is_inside_; }

        void set_is_inside(bool v) { is_inside_ = v; }

        const String &bend_digit() const { return bend_digit_; }

        void set_bend_digit(const String &s) { bend_digit_ = s; }

        const std::set<int> &dive_levels() const { return dive_levels_; }

        void reset_dive_levels() { dive_levels_.clear(); }

        void set_dive_levels(const std::set<int> &v) { dive_levels_ = v; }

        bool above_staff() const { return above_staff_; }

        void set_above_staff(bool v) { above_staff_ = v; }

    private:
        bool up_ = true;
        bool is_inside_ = false;
        String bend_digit_;
        std::set<int> dive_levels_;
        bool above_staff_ = false;
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBend)

protected:
    bool is_in_spanner_map() const override { return false; }

private:
    GuitarBendType bend_type_ = GuitarBendType::BEND;
    GuitarBendHold *hold_line_ = nullptr;
    bool is_invalid_ = false;
    bool is_borderline_unplayable_ = false;
    DirectionV dive_tab_pos_ = DirectionV::AUTO;
    VibratoType dip_vibrato_type_ = VibratoType::NONE;
    bool is_slack_ = false;
    std::optional<float> target_time_factor_;
};

struct GuitarBendText; // forward decl

struct GuitarBendSegment final : LineSegment {
private:
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_SEGMENT)

public:
    GuitarBendSegment(GuitarBend *sp);
    GuitarBendSegment(const GuitarBendSegment &);
    ~GuitarBendSegment() override;

    GuitarBend *guitar_bend() const { return to_guitar_bend(spanner()); }

    GuitarBendSegment *clone() const override { return new GuitarBendSegment(*this); }

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;
    EngravingObject *property_delegate(Pid id) const override;

    PointF vertex_point_off() const { return vertex_point_off_; }

    void set_vertex_point_off(PointF p) { vertex_point_off_ = p; }

    void reset() override;

    double line_width() const;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    GuitarBendText *bend_text() const { return text_; }

    void set_bend_text(GuitarBendText *t) { text_ = t; }

    bool is_user_modified() const override;

    Color cur_color(const rendering::PaintOptions &opt) const override { return guitar_bend()->cur_color(opt); }

    struct LayoutData : public LineSegment::LayoutData {
    public:
        const PointF &vertex_point() const { return vertex_point_; }

        void set_vertex_point(PointF p) { vertex_point_ = p; }

        const PainterPath &path() const { return path_; }

        void set_path(const PainterPath &path) { path_ = path; }

        const PolygonF &arrow() const { return arrow_; }

        PolygonF &arrow() { return arrow_; }

        void set_arrow(const PolygonF &a) { arrow_ = a; }

    private:
        PointF vertex_point_ = PointF(); // Coordinates relative to pos()
        PainterPath path_ = PainterPath();
        PolygonF arrow_ = PolygonF();
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBendSegment)

private:

    PointF vertex_point_off_ = PointF();
    GuitarBendText *text_ = nullptr;
};

struct GuitarBendHold final : SLine {
private:
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_HOLD)

public:
    GuitarBendHold(GuitarBend *parent);
    GuitarBendHold(const GuitarBendHold &);

    GuitarBendHold *clone() const override { return new GuitarBendHold(*this); }

    Anchor anchor() const override { return Anchor::NOTE; }

    LineSegment *create_line_segment() override;

    PropertyValue property_default(Pid id) const override;

    bool allow_time_anchor() const override { return false; }

    Note *start_note() const;
    Note *end_note() const;

    GuitarBend *guitar_bend() const { return to_guitar_bend(ownership_parent()); }

    double line_width() const;

protected:
    bool is_in_spanner_map() const override { return false; }
};

struct GuitarBendHoldSegment final : LineSegment {
private:
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_HOLD_SEGMENT)

public:
    GuitarBendHoldSegment(GuitarBendHold *sp);

    GuitarBendHold *guitar_bend_hold() const { return to_guitar_bend_hold(spanner()); }

    GuitarBendHoldSegment *clone() const override { return new GuitarBendHoldSegment(*this); }

    GuitarBendSegment *start_bend_seg() const { return start_bend_seg_; }

    GuitarBendSegment *end_bend_seg() const { return end_bend_seg_; }

    void set_start_bend_seg(GuitarBendSegment *s) { start_bend_seg_ = s; }

    void set_end_bend_seg(GuitarBendSegment *s) { end_bend_seg_ = s; }

    double dash_length() const { return dash_length_; }

    double line_width() const { return guitar_bend_hold()->line_width(); }

    struct LayoutData : LineSegment::LayoutData {
    public:
        const SymIdList &sym_ids() const { return sym_ids_; }

        void set_sym_ids(const SymIdList &v) { sym_ids_ = v; }

    private:
        SymIdList sym_ids_;
    };
    DECLARE_LAYOUTDATA_METHODS(GuitarBendHoldSegment)

private:

    double dash_length_ = 3.0;

    GuitarBendSegment *start_bend_seg_ = nullptr;
    GuitarBendSegment *end_bend_seg_ = nullptr;
};

struct GuitarBendText final : TextBase {
private:
    DECLARE_CLASSOF(ElementType::GUITAR_BEND_TEXT)

public:
    GuitarBendText(GuitarBendSegment *parent);

    GuitarBendText *clone() const override { return new GuitarBendText(*this); }

    bool position_relative_to_notehead_rest() const override { return false; }
};
} // namespace mu::engraving
