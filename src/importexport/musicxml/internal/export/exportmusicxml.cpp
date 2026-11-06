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

/**
 MusicXML export.
 */

//=========================================================
//  LVI FIXME
//
//  Evaluate parameter handling between the various classes, could be simplified
//=========================================================

// TODO LVI 2011-10-30: determine how to report export errors.
// Currently all output (both debug and error reports) are done using LOGD.

#include <cassert>
#include "exportmusicxml.h"

#include <math.h>
#include <set>

#include "containers.h"
#include "realfn.h"
#include "io/iodevice.h"
#include "io/buffer.h"
#include "io/file.h"
#include "io/fileinfo.h"
#include "global/serialization/xmlstreamreader.h"
#include "global/serialization/zipwriter.h"

#include "engraving/style/style.h"
#include "engraving/rw/xmlwriter.h"
#include "engraving/types/typesconv.h"
#include "engraving/types/symnames.h"

#include "engraving/dom/accidental.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/beam.h"
#include "engraving/dom/box.h"
#include "engraving/dom/bracket.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/figuredbass.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/gradualtempochange.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/hammeronpulloff.h"
#include "engraving/dom/harmonicmark.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/image.h"
#include "engraving/dom/imageStore.h"
#include "engraving/dom/harppedaldiagram.h"
#include "engraving/dom/instrchange.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/laissezvib.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/letring.h"
#include "engraving/dom/linkedobjects.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/page.h"
#include "engraving/dom/palmmute.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pedal.h"
#include "engraving/dom/pickscrape.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/playtechannotation.h"
#include "engraving/dom/rasgueado.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stem.h"
#include "engraving/dom/stringdata.h"
#include "engraving/dom/system.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/textlinebase.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tremolotwochord.h"
#include "engraving/dom/trill.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/utils.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/whammybar.h"

#include "../shared/musicxmlfonthandler.h"
#include "../shared/musicxmlsupport.h"
#include "../shared/musicxmltypes.h"

#include "imusicxmlconfiguration.h"
#include "global/appinfo.h"
#include "global/realfn.h"
#include "engraving/iengravingconfiguration.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;
using namespace mu::iex::musicxml;
using namespace mu::engraving;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   local defines for debug output
//---------------------------------------------------------

// #define DEBUG_CLEF true
// #define DEBUG_REPEATS true
// #define DEBUG_TICK true

#ifdef DEBUG_CLEF
#define clefDebug(...) LOGD(__VA_ARGS__)
#else
#define clefDebug(...) \
    {}
#endif

constexpr int max_part_groups = 8;

//---------------------------------------------------------
//   typedefs
//---------------------------------------------------------

typedef std::map<track_idx_t, const FiguredBass *> FigBassMap;

//---------------------------------------------------------
//   attributes -- prints <attributes> tag when necessary
//---------------------------------------------------------

struct Attributes {
    Attributes() { start(); }

    void do_attr(XmlWriter &xml, bool attr);
    void start();
    void stop(XmlWriter &xml);

private:
    bool in_attributes_ = false;
};

//---------------------------------------------------------
//   doAttr - when necessary change state and print <attributes> tag
//---------------------------------------------------------

void Attributes::do_attr(XmlWriter &xml, bool attr) {
    if (!in_attributes_ && attr) {
        xml.start_element("attributes");
        in_attributes_ = true;
    } else if (in_attributes_ && !attr) {
        xml.end_element();
        in_attributes_ = false;
    }
}

//---------------------------------------------------------
//   start -- initialize
//---------------------------------------------------------

void Attributes::start() { in_attributes_ = false; }

//---------------------------------------------------------
//   stop -- print </attributes> tag when necessary
//---------------------------------------------------------

void Attributes::stop(XmlWriter &xml) {
    if (in_attributes_) {
        xml.end_element();
        in_attributes_ = false;
    }
}

//---------------------------------------------------------
//   notations -- prints <notations> tag when necessary
//---------------------------------------------------------

struct Notations {
    void tag(XmlWriter &xml, const EngravingItem *e, const AsciiStringView &container = "");
    void etag(XmlWriter &xml);
    void close_container(XmlWriter &xml);

private:
    bool notations_printed_ = false;
    bool prev_element_visible_ = true;
    AsciiStringView current_container_;
};

//---------------------------------------------------------
//   slur handler -- prints <slur> tags
//---------------------------------------------------------

struct SlurHandler {
    SlurHandler();

    void set_export_layout(bool export_layout) { export_layout_ = export_layout; }

    void do_slurs(const ChordRest *chord_rest, Notations &notations, XmlWriter &xml);

private:
    void do_slur_start(const Slur *s, Notations &notations, String tag_name, XmlWriter &xml);
    void do_slur_stop(const Slur *s, Notations &notations, String tag_name, XmlWriter &xml);
    int find_slur(const Slur *s) const;

    const Slur *slur_[::mu::iex::musicxml::max_number_level];
    bool started_[::mu::iex::musicxml::max_number_level];
    bool export_layout_ = true;
};

//---------------------------------------------------------
//   glissando handler -- prints <glissando> tags
//---------------------------------------------------------

struct GlissandoHandler {
    GlissandoHandler();
    void reset();
    void do_glissando_start(Glissando *gliss, Notations &notations, XmlWriter &xml);
    void do_glissando_stop(Glissando *gliss, Notations &notations, XmlWriter &xml);

private:
    int find_note(const Note *note, int type) const;

    const Note *gliss_note_[::mu::iex::musicxml::max_number_level];
    const Note *slide_note_[::mu::iex::musicxml::max_number_level];
};

//---------------------------------------------------------
//  MeasureNumberStateHandler
//---------------------------------------------------------

/**
 State handler used to calculate measure number including implicit flag.
 To be called once at the start of each measure in a part.
 */

struct MeasureNumberStateHandler final {
    MeasureNumberStateHandler();
    void update_for_measure(const Measure *const m);
    String measure_number() const;
    bool is_first_actual_measure() const;
private:
    void init();
    int measure_number_ = 0;           // number of next regular measure
    int measure_number_offset_ = 0;    // measure number offset
    int irregular_measure_number_ = 0; // number of next irregular measure
    int pickup_measure_number_ = 0;    // number of next pickup measure
    String cached_attributes_;         // attributes calculated by updateForMeasure()
};

//---------------------------------------------------------
//   MeasurePrintContext
//---------------------------------------------------------

struct MeasurePrintContext final {
    void measure_written(const Measure *m);
    bool score_start = true;
    bool page_start = true;
    bool system_start = true;
    size_t page_number = 0;
    bool write_page_no = false;
    const Measure *prev_measure = nullptr;
    const System *prev_system = nullptr;
    const System *last_system_prev_page = nullptr;
};

//---------------------------------------------------------
//   ExportMusicXml
//---------------------------------------------------------

typedef std::unordered_map<const ChordRest *, const Trill *> TrillHash;
typedef std::map<const Instrument *, int> MusicXmlInstrumentMap;

// The configuration of the export in progress. ExportMusicXml binds it for its
// lifetime because the static position helpers reach it without an exporter;
// exports run one at a time on the engine's owner thread.
static const IMusicXmlConfiguration *s_export_configuration = nullptr;

struct ExportMusicXml {
    ExportMusicXml(Score *s, const IMusicXmlConfiguration &configuration) : configuration_(configuration) {
        assert(!s_export_configuration);
        s_export_configuration = &configuration;
        sh_.set_export_layout(configuration.export_layout());
        score_ = s;
        tick_ = {0, 1};
        div_ = 1;
        tenths_ = 40;
        millimeters_ = score_->style().spatium() * tenths_ / (10 * ::mu::engraving::dpmm);
    }

    ~ExportMusicXml() { s_export_configuration = nullptr; }

    ExportMusicXml(const ExportMusicXml &) = delete;
    ExportMusicXml &operator=(const ExportMusicXml &) = delete;

    static const IMusicXmlConfiguration *configuration() { return s_export_configuration; }

    void write(muse::io::IODevice *dev);
    void credits(XmlWriter &xml);
    void move_to_tick(const Fraction &t, const Fraction &stretch = {1, 1});
    void move_to_tick_if_need(const Fraction &t, track_idx_t track, const Fraction &measure_tick);
    void words(TextBase const *const text, staff_idx_t staff);
    void tbox_text_as_words(TextBase const *const text, const staff_idx_t staff, PointF position);
    void image(const Image *const img, staff_idx_t staff);
    void rehearsal(RehearsalMark const *const rmk, staff_idx_t staff);
    void harp_pedals(HarpPedalDiagram const *const hpd, staff_idx_t staff);
    void hairpin(Hairpin const *const hp, staff_idx_t staff, const Fraction &tick);
    void ottava(Ottava const *const ot, staff_idx_t staff, const Fraction &tick);
    void pedal(Pedal const *const pd, staff_idx_t staff, const Fraction &tick);
    void play_text(PlayTechAnnotation const *const annot, staff_idx_t staff);
    void text_line(TextLineBase const *const tl, staff_idx_t staff, const Fraction &tick);
    void dynamic(Dynamic const *const dyn, staff_idx_t staff);
    void system_text(StaffTextBase const *const text, staff_idx_t staff);
    void tempo_text(TempoText const *const text, staff_idx_t staff);
    void swing_sound(StaffTextBase const *const text, const bool offset = false);
    void tempo_sound(TempoText const *const text);
    void harmony(Harmony const *const, FretDiagram const *const fd, const Fraction &offset = Fraction(0, 1));

    Score *score() const { return score_; }

    void set_mxl(bool v) { is_mxl_ = v; }

    double get_tenths_from_inches(double) const;
    double get_tenths_from_dots(double) const;

    Fraction tick() const { return tick_; }

    void write_instrument_change(const InstrumentChange *instr_change);
    void write_instrument_details(const Instrument *instrument, const bool concert_pitch);

    static bool can_write(const EngravingItem *e);

    static String positioning_attributes(EngravingItem const *const el, bool is_span_start = true);
    static String fermata_position(const Fermata *const fermata);

private:
    int find_bracket(const TextLineBase *tl) const;
    int find_dashes(const TextLineBase *tl) const;
    int find_hairpin(const Hairpin *tl) const;
    int find_ottava(const Ottava *tl) const;
    int find_trill(const Trill *tl) const;
    void chord(Chord *chord, staff_idx_t staff, const std::vector<Lyrics *> &ll, bool use_drumset);
    void rest(Rest *chord, staff_idx_t staff, const std::vector<Lyrics *> &ll);
    void clef(staff_idx_t staff, const ClefType ct, const String &extra_attributes = u"");
    void timesig(const TimeSig *tsig, staff_idx_t staff);
    void keysig(const KeySig *ks, ClefType ct, staff_idx_t staff = 0);
    void barline_left(const Measure *const m, const track_idx_t track);
    void barline_middle(const BarLine *bl);
    void barline_right(const Measure *const m, const track_idx_t strack, const track_idx_t etrack);
    void lyrics(const std::vector<Lyrics *> &ll, const track_idx_t trk);
    void work(const MeasureBase *measure);
    void calc_div_move_to_tick(const Fraction &t, const Fraction &stretch = {1, 1});
    void calc_divisions();
    void keysig_timesig(const Measure *m, const Part *p);
    void chord_attributes(Chord *chord, Notations &notations, TrillHash &trill_start, TrillHash &trill_stop);
    void wavy_line_start_stop(const ChordRest *cr, Notations &notations, TrillHash &trill_start, TrillHash &trill_stop);
    void print(const Measure *const m, const int part_nr, const int first_staff_of_part, const size_t nr_staves_in_part,
               const MeasurePrintContext &mpc);
    void measure_layout(const double distance);
    void find_and_export_clef(const Measure *const m, const int staves, const track_idx_t strack, const track_idx_t etrack);
    void export_default_clef(const Part *const part, const Measure *const m);
    void write_element(EngravingItem *el, const Measure *m, staff_idx_t sstaff, bool use_drumset);
    void write_measure_tracks(const Measure *const m, const int part_index, const staff_idx_t strack, const staff_idx_t part_rel_staff_no,
                              const bool use_drumset, const bool is_last_staff_of_part, FigBassMap &fb_map,
                              std::set<const Spanner *> &spanners_stopped);
    void write_measure_staves(const Measure *m, const int part_index, const staff_idx_t start_staff, const size_t nstaves,
                              const bool use_drumset, FigBassMap &fb_map, std::set<const Spanner *> &spanners_stopped);
    void write_measure(const Measure *const m, const int idx, const int staff_count, MeasureNumberStateHandler &mnsh, FigBassMap &fb_map,
                       const MeasurePrintContext &mpc, std::set<const Spanner *> &spanners_stopped);
    void repeat_at_measure_start(Attributes &attr, const Measure *const m, track_idx_t strack, track_idx_t etrack, track_idx_t track);
    void repeat_at_measure_stop(const Measure *const m, track_idx_t strack, track_idx_t etrack, track_idx_t track);
    void write_parts();
    bool should_write_page_no(const Page *page);

    static String element_position(const ExportMusicXml *const exp_mxml, const EngravingItem *const elm);
    static String positioning_attributes_for_tbox_text(const PointF position, float spatium);
    void identification(XmlWriter &xml, Score const *const score);

    Score *score_ = nullptr;
    XmlWriter xml_;
    const IMusicXmlConfiguration &configuration_;
    SlurHandler sh_;
    GlissandoHandler gh_;
    Fraction tick_;
    Attributes attr_;
    const TextLineBase *brackets_[::mu::iex::musicxml::max_number_level];
    const TextLineBase *dashes_[::mu::iex::musicxml::max_number_level];
    const Hairpin *hairpins_[::mu::iex::musicxml::max_number_level];
    const Ottava *ottavas_[::mu::iex::musicxml::max_number_level];
    const Trill *trills_[::mu::iex::musicxml::max_number_level];
    std::vector<const Jump *> jump_elements_;
    ArpeggioMap meas_arpeggios_;
    int div_ = 0;
    double millimeters_ = 0.0;
    int tenths_ = 0;
    bool tboxes_above_written_ = false;
    bool tboxes_below_written_ = false;
    std::vector<size_t> hidden_staves_;
    TrillHash trill_start_;
    TrillHash trill_stop_;
    MusicXmlInstrumentMap instr_map_;
    PlayingTechniqueType curr_play_technique_ = PlayingTechniqueType::UNDEFINED;
    bool is_mxl_ = false;
};

//---------------------------------------------------------
//   fractionToStdString
//---------------------------------------------------------

#ifdef DEBUG_TICK
static std::string fractionToStdString(const Fraction &f) {
    if (!f.is_valid()) {
        return "<invalid>";
    }
    String res{f.to_string()};
    res += String(u" (%1)").arg(String::number(f.ticks()));
    return res.to_std_string();
}

#endif

//---------------------------------------------------------
//   durElemTicksToStdString
//---------------------------------------------------------

#ifdef DEBUG_TICK
static std::string durElemTicksToStdString(const DurationElement &d) {
    String res;
    res += String(u" ticks %1").arg(d.ticks().to_string());
    res += String(u" (%1)").arg(String::number(d.ticks().ticks()));
    res += String(u" globalTicks %1").arg(d.global_ticks().to_string());
    res += String(u" (%1)").arg(String::number(d.global_ticks().ticks()));
    res += String(u" actualTicks %1").arg(d.actual_ticks().to_string());
    res += String(u" (%1)").arg(String::number(d.actual_ticks().ticks()));
    return res.to_std_string();
}

#endif

//---------------------------------------------------------
//   positionToString
//---------------------------------------------------------

static String position_to_string(const PointF def, const PointF rel, const float spatium) {
    // minimum value to export
    const float position_elipson = 0.1f;

    // convert into tenths for MusicXML
    const float default_x = 10 * def.x() / spatium;
    const float default_y = -10 * def.y() / spatium;
    const float relative_x = 10 * rel.x() / spatium;
    const float relative_y = -10 * rel.y() / spatium;

    // generate string representation
    String res;
    if (fabsf(default_x) > position_elipson) {
        res += String(u" default-x=\"%1\"").arg(String::number(default_x, 2));
    }
    if (fabsf(default_y) > position_elipson) {
        res += String(u" default-y=\"%1\"").arg(String::number(default_y, 2));
    }
    if (fabsf(relative_x) > position_elipson) {
        res += String(u" relative-x=\"%1\"").arg(String::number(relative_x, 2));
    }
    if (fabsf(relative_y) > position_elipson) {
        res += String(u" relative-y=\"%1\"").arg(String::number(relative_y, 2));
    }

    return res;
}

//---------------------------------------------------------
//   positioningAttributes
//   According to the specs (common.dtd), all direction-type and note elements must be relative to the measure
//   while all other elements are relative to their position or the nearest note.
//---------------------------------------------------------

String ExportMusicXml::positioning_attributes(EngravingItem const *const el, bool is_span_start) {
    if (!configuration()->export_layout()) {
        return String();
    }

    //LOGD("single el %p _pos x,y %f %f _userOff x,y %f %f spatium %f",
    //       el, el->ldata()->pos.x(), el->ldata()->pos.y(), el->offset().x(), el->offset().y(), el->spatium());

    PointF def;
    PointF rel;
    float spatium = el->spatium();

    const SLine *span = el->is_s_line() ? to_s_line(el) : nullptr;

    if (span && !span->segments_empty()) {
        if (is_span_start) {
            const LineSegment *seg = span->front_segment();
            const PointF offset = seg->offset();
            const PointF p = seg->pos();
            rel.set_x(offset.x());
            def.set_y(p.y());

            //LOGD("sline start seg %p seg->pos x,y %f %f seg->userOff x,y %f %f spatium %f",
            //       seg, p.x(), p.y(), seg->offset().x(), seg->offset().y(), seg->spatium());
        } else {
            const LineSegment *seg = span->back_segment();
            const PointF user_off = seg->offset(); // This is the offset accessible from the Properties panel
            const PointF user_off2 =
                seg->user_off2(); // Offset of the actual dragged anchor, which doesn't affect the Properties panel offset
            //auto pos = seg->pos();
            //auto pos2 = seg->pos2();

            //LOGD("sline stop seg %p seg->pos2 x,y %f %f seg->userOff2 x,y %f %f spatium %f",
            //       seg, pos2.x(), pos2.y(), seg->userOff2().x(), seg->userOff2().y(), seg->spatium());

            // For an SLine, the actual offset equals the sum of userOff and userOff2,
            // as userOff moves the SLine as a whole
            rel.set_x(user_off.x() + user_off2.x());

            // Following would probably required for non-horizontal SLines:
            //defaultY = pos.y() + pos2.y();
        }
    } else {
        def = el->ldata()->pos(); // Note: for some elements, Finale Notepad seems to work slightly better w/o default-x
        rel = el->offset();
    }

    return position_to_string(def, rel, spatium);
}

//---------------------------------------------------------
//   tag
//---------------------------------------------------------

void Notations::tag(XmlWriter &xml, const EngravingItem *e, const AsciiStringView &container) {
    if (notations_printed_ && prev_element_visible_ != e->visible()) {
        etag(xml);
    }

    if (!notations_printed_) {
        if (e->visible()) {
            xml.start_element("notations");
        } else {
            xml.start_element("notations", {{"print-object", "no"}});
        }
        if (!container.empty() && (current_container_ == container)) {
            // restart container to ensure it's inside notations
            xml.start_element(container);
        }
    }
    if (current_container_ != container) {
        if (!current_container_.empty()) {
            xml.end_element();
        }
        if (!container.empty()) {
            xml.start_element(container);
        }
    }

    current_container_ = container;
    notations_printed_ = true;
    prev_element_visible_ = e->visible();
}

//---------------------------------------------------------
//   etag
//---------------------------------------------------------

void Notations::etag(XmlWriter &xml) {
    if (!current_container_.empty()) {
        xml.end_element();
        current_container_ = "";
    }
    if (notations_printed_) {
        xml.end_element();
    }
    notations_printed_ = false;
}

//---------------------------------------------------------
//   closeContainer
//---------------------------------------------------------

void Notations::close_container(XmlWriter &xml) {
    if (!current_container_.empty()) {
        xml.end_element();
    }
    current_container_ = "";
}

//---------------------------------------------------------
//   color2xml
//---------------------------------------------------------

static String color2xml(const EngravingItem *el) {
    if (el->color() != el->configuration()->default_color()) {
        return String(u" color=\"%1\"").arg(String::from_std_string(el->color().to_string()));
    } else if (el->is_s_line() && ((SLine *) el)->line_color() != el->configuration()->default_color()) {
        return String(u" color=\"%1\"").arg(String::from_std_string(((SLine *) el)->line_color().to_string()));
    } else {
        return String();
    }
}

static void add_color_attr(const EngravingItem *el, XmlWriter::Attributes &attrs) {
    if (el->color() != el->configuration()->default_color()) {
        attrs.emplace_back(std::make_pair("color", String::from_std_string(el->color().to_string())));
    }
}

//---------------------------------------------------------
//   frame2xml
//---------------------------------------------------------

static String frame2xml(const TextBase *el) {
    switch (el->frame_type()) {
        case FrameType::CIRCLE: return u" enclosure=\"circle\"";
        case FrameType::RECTANGLE: return u" enclosure=\"rectangle\"";
        default: return String();
    }
}

//---------------------------------------------------------
//   fontStyleToXML
//---------------------------------------------------------

static String font_style_to_xml(const FontStyle style, bool allow_underline = true) {
    String res;
    if (style & FontStyle::BOLD) {
        res += u" font-weight=\"bold\"";
    }
    if (style & FontStyle::ITALIC) {
        res += u" font-style=\"italic\"";
    }
    if (allow_underline && style & FontStyle::UNDERLINE) {
        res += u" underline=\"1\"";
    }
    // at places where underline is not wanted (e.g. fingering, pluck), strike is not wanted too
    if (allow_underline && style & FontStyle::STRIKE) {
        res += u" line-through=\"1\"";
    }
    return res;
}

//---------------------------------------------------------
//   placement2xml
//---------------------------------------------------------

static String placement2xml(const EngravingItem *el) {
    return String(u" placement=\"%1\"").arg(String::from_ascii(TConv::to_xml(el->placement()).ascii()));
}

//---------------------------------------------------------
//   slurHandler
//---------------------------------------------------------

SlurHandler::SlurHandler() {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        slur_[i] = 0;
        started_[i] = false;
    }
}

static String slur_tie_line_style(const SlurTie *s) {
    String line_type;
    String rest;
    switch (s->style_type()) {
        case SlurStyleType::DASHED:
        case SlurStyleType::WIDE_DASHED: line_type = u"dashed"; break;
        case SlurStyleType::DOTTED: line_type = u"dotted"; break;
        case SlurStyleType::SOLID:
        default: line_type.clear();
    }
    if (!line_type.is_empty()) {
        rest = String(u" line-type=\"%1\"").arg(line_type);
    }
    if (s->slur_direction() != engraving::DirectionV::AUTO) {
        rest += String(u" orientation=\"%1\"").arg(s->up() ? u"over" : u"under");
        rest += String(u" placement=\"%1\"").arg(s->up() ? u"above" : u"below");
    }
    rest += color2xml(s);
    return rest;
}

static String slur_tie_bezier(const SlurTie *st, const bool start, bool export_layout) {
    if (!export_layout) {
        return String();
    }

    String attribute_string;
    const int spatium = st->spatium();
    if (!st->spanner_segments().empty()) {
        if (start) {
            const SlurTieSegment *front = to_slur_tie_segment(st->front_segment());
            const PointF start_p = front->ups(Grip::START).pos();
            const PointF bezier_p = front->ups(Grip::BEZIER1).pos();
            attribute_string += String(u" bezier-x=\"%1\"").arg(10 * (bezier_p.x() - start_p.x()) / spatium);
            attribute_string += String(u" bezier-y=\"%1\"").arg(-10 * (bezier_p.y() - start_p.y()) / spatium);
        } else {
            const SlurTieSegment *back = to_slur_tie_segment(st->back_segment());
            const PointF end_p = back->ups(Grip::END).pos();
            const PointF bezier_p = back->ups(Grip::BEZIER2).pos();
            attribute_string += String(u" bezier-x=\"%1\"").arg(10 * (bezier_p.x() - end_p.x()) / spatium);
            attribute_string += String(u" bezier-y=\"%1\"").arg(-10 * (bezier_p.y() - end_p.y()) / spatium);
        }
    }

    return attribute_string;
}

//---------------------------------------------------------
//   findSlur -- get index of slur in slur table
//   return -1 if not found
//---------------------------------------------------------

int SlurHandler::find_slur(const Slur *s) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (slur_[i] == s) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   findFirstChordRest -- find first chord or rest (in musical order) for slur s
//   note that this is not necessarily the same as s->startElement()
//---------------------------------------------------------

static const ChordRest *find_first_chord_rest(const Slur *s) {
    const EngravingItem *e1 = s->start_element();
    if (!e1 || !(e1->is_chord_rest())) {
        LOGD("no valid start element for slur %p", s);
        return nullptr;
    }

    const EngravingItem *e2 = s->end_element();
    if (!e2 || !(e2->is_chord_rest())) {
        LOGD("no valid end element for slur %p", s);
        return nullptr;
    }

    if (e1->tick() < e2->tick()) {
        return to_chord_rest(e1);
    } else if (e1->tick() > e2->tick()) {
        return to_chord_rest(e2);
    }

    if (e1->is_rest() || e2->is_rest()) {
        return nullptr;
    }

    const Chord *c1 = to_chord(e1);
    const Chord *c2 = to_chord(e2);

    // c1->tick() == c2->tick()
    if (!c1->is_grace() && !c2->is_grace()) {
        // slur between two regular notes at the same tick
        // probably shouldn't happen but handle just in case
        LOGD("invalid slur between chords %p and %p at tick %d", c1, c2, c1->tick().ticks());
        return 0;
    } else if (c1->is_grace_before() && !c2->is_grace_before()) {
        return c1; // easy case: c1 first
    } else if (c1->is_grace_after() && !c2->is_grace_after()) {
        return c2; // easy case: c2 first
    } else if (c2->is_grace_before() && !c1->is_grace_before()) {
        return c2; // easy case: c2 first
    } else if (c2->is_grace_after() && !c1->is_grace_after()) {
        return c1; // easy case: c1 first
    } else {
        // both are grace before or both are grace after -> compare grace indexes
        // (note: higher means closer to the non-grace chord it is attached to)
        if ((c1->is_grace_before() && c1->grace_index() < c2->grace_index()) ||
            (c1->is_grace_after() && c1->grace_index() > c2->grace_index())) {
            return c1;
        } else {
            return c2;
        }
    }
}

//---------------------------------------------------------
//   doSlurs
//---------------------------------------------------------

void SlurHandler::do_slurs(const ChordRest *chord_rest, Notations &notations, XmlWriter &xml) {
    // loop over all slurs twice, first to handle the stops, then the starts
    for (int i = 0; i < 2; ++i) {
        // search for slur(s) starting or stopping at this chord
        for (const auto &it : chord_rest->score()->spanner()) {
            String tag_name = u"slur";
            auto sp = it.second;
            if (sp->generated() || !sp->is_slur()) {
                continue;
            }
            if (sp->is_hammer_on_pull_off() && !sp->spanner_segments().empty()) {
                const HammerOnPullOffSegment *hopo_seg = to_hammer_on_pull_off_segment(sp->spanner_segments().front());
                if (!hopo_seg->hopo_text().empty()) {
                    tag_name = hopo_seg->hopo_text().front()->is_hammer_on() ? u"hammer-on" : u"pull-off";
                }
            }
            if (chord_rest == sp->start_element() || chord_rest == sp->end_element()) {
                const Slur *s = to_slur(sp);
                const ChordRest *first_chord_rest = find_first_chord_rest(s);
                if (first_chord_rest) {
                    if (i == 0) {
                        // first time: do slur stops
                        if (first_chord_rest != chord_rest) {
                            do_slur_stop(s, notations, tag_name, xml);
                        }
                    } else {
                        // second time: do slur starts
                        if (first_chord_rest == chord_rest) {
                            do_slur_start(s, notations, tag_name, xml);
                        }
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//   doSlurStart
//---------------------------------------------------------

void SlurHandler::do_slur_start(const Slur *s, Notations &notations, String tag_name, XmlWriter &xml) {
    // only slurs can hold line styles
    const bool style = (tag_name == u"slur");
    // check if on slur list (i.e. stop already seen)
    int i = find_slur(s);
    // compose tag
    tag_name += u" type=\"start\"";
    tag_name += style ? slur_tie_line_style(s) : color2xml(s);
    tag_name += slur_tie_bezier(s, true, export_layout_);

    if (i >= 0) {
        // remove from list and print start
        slur_[i] = 0;
        started_[i] = false;
        notations.tag(xml, s);
        tag_name += String(u" number=\"%1\"").arg(i + 1);
        xml.tag_raw(tag_name);
    } else {
        // find free slot to store it
        i = find_slur(0);
        if (i >= 0) {
            slur_[i] = s;
            started_[i] = true;
            notations.tag(xml, s);
            tag_name += String(u" number=\"%1\"").arg(i + 1);
            xml.tag_raw(tag_name);
        } else {
            LOGD("no free slur slot");
        }
    }
}

//---------------------------------------------------------
//   doSlurStop
//---------------------------------------------------------

// Note: a slur may start in a higher voice in the same measure.
// In that case it is not yet started (i.e. on the active slur list)
// when doSlurStop() is executed. Handle this slur as follows:
// - generate stop anyway and put it on the slur list
// - doSlurStart() starts slur but doesn't store it

void SlurHandler::do_slur_stop(const Slur *s, Notations &notations, String tag_name, XmlWriter &xml) {
    // check if on slur list
    int i = find_slur(s);
    if (i < 0) {
        // if not, find free slot to store it
        i = find_slur(0);
        if (i >= 0) {
            slur_[i] = s;
            started_[i] = false;
            notations.tag(xml, s);
            tag_name += String(u" type=\"stop\" number=\"%1\"").arg(i + 1);
            tag_name += slur_tie_bezier(s, false, export_layout_);
            xml.tag_raw(tag_name);
        } else {
            LOGD("no free slur slot");
        }
    } else {
        // found (already started), stop it and remove from list
        slur_[i] = 0;
        started_[i] = false;
        notations.tag(xml, s);
        tag_name += String(u" type=\"stop\" number=\"%1\"").arg(i + 1);
        tag_name += slur_tie_bezier(s, false, export_layout_);
        xml.tag_raw(tag_name);
    }
}

//---------------------------------------------------------
//   glissando
//---------------------------------------------------------

// <notations>
//   <slide line-type="solid" number="1" type="start"/>
//   </notations>

// <notations>
//   <glissando line-type="wavy" number="1" type="start"/>
//   </notations>

static void glissando(const Glissando *gli, int number, bool start, Notations &notations, XmlWriter &xml) {
    String tag_name;
    if (gli->glissando_type() == GlissandoType::STRAIGHT) {
        switch (gli->line_style()) {
            case LineType::SOLID: tag_name = u"slide line-type=\"solid\""; break;
            case LineType::DASHED: tag_name = u"slide line-type=\"dashed\""; break;
            case LineType::DOTTED: tag_name = u"slide line-type=\"dotted\""; break;
        }
    } else {
        tag_name = u"glissando line-type=\"wavy\"";
    }
    tag_name += String(u" number=\"%1\" type=\"%2\"").arg(number).arg(start ? u"start" : u"stop");
    if (start) {
        tag_name += color2xml(gli);
        tag_name += ExportMusicXml::positioning_attributes(gli, start);
    }
    notations.tag(xml, gli);
    if (start && gli->show_text() && !gli->text().empty()) {
        xml.tag_raw(tag_name, gli->text());
    } else {
        xml.tag_raw(tag_name);
    }
}

//---------------------------------------------------------
//   GlissandoHandler
//---------------------------------------------------------

GlissandoHandler::GlissandoHandler() { reset(); }

void GlissandoHandler::reset() {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        gliss_note_[i] = 0;
        slide_note_[i] = 0;
    }
}

//---------------------------------------------------------
//   findNote -- get index of Note in note table for subtype type
//   return -1 if not found
//---------------------------------------------------------

int GlissandoHandler::find_note(const Note *note, int type) const {
    if (type != 0 && type != 1) {
        LOGD("GlissandoHandler::findNote: unknown glissando subtype %d", type);
        return -1;
    }
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (type == 0 && slide_note_[i] == note) {
            return i;
        }
        if (type == 1 && gliss_note_[i] == note) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   doGlissandoStart
//---------------------------------------------------------

void GlissandoHandler::do_glissando_start(Glissando *gliss, Notations &notations, XmlWriter &xml) {
    GlissandoType type = gliss->glissando_type();
    if (type != GlissandoType::STRAIGHT && type != GlissandoType::WAVY) {
        LOGD("doGlissandoStart: unknown glissando subtype %d", int(type));
        return;
    }
    Note *note = to_note(gliss->start_element());
    // check if on chord list
    int i = find_note(note, int(type));
    if (i >= 0) {
        // print error and remove from list
        LOGD("doGlissandoStart: note for glissando/slide %p already on list", gliss);
        if (type == GlissandoType::STRAIGHT) {
            slide_note_[i] = 0;
        }
        if (type == GlissandoType::WAVY) {
            gliss_note_[i] = 0;
        }
    }
    // find free slot to store it
    i = find_note(0, int(type));
    if (i >= 0) {
        if (type == GlissandoType::STRAIGHT) {
            slide_note_[i] = note;
        }
        if (type == GlissandoType::WAVY) {
            gliss_note_[i] = note;
        }
        glissando(gliss, i + 1, true, notations, xml);
    } else {
        LOGD("doGlissandoStart: no free slot");
    }
}

//---------------------------------------------------------
//   doGlissandoStop
//---------------------------------------------------------

void GlissandoHandler::do_glissando_stop(Glissando *gliss, Notations &notations, XmlWriter &xml) {
    GlissandoType type = gliss->glissando_type();
    if (type != GlissandoType::STRAIGHT && type != GlissandoType::WAVY) {
        LOGD("doGlissandoStart: unknown glissando subtype %d", int(type));
        return;
    }
    Note *note = to_note(gliss->start_element());
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (type == GlissandoType::STRAIGHT && slide_note_[i] == note) {
            slide_note_[i] = 0;
            glissando(gliss, i + 1, false, notations, xml);
            return;
        }
        if (type == GlissandoType::WAVY && gliss_note_[i] == note) {
            gliss_note_[i] = 0;
            glissando(gliss, i + 1, false, notations, xml);
            return;
        }
    }
    LOGD("doGlissandoStop: glissando note %p not found", note);
}

//---------------------------------------------------------
//   directions anchor -- anchor directions at another element or a specific tick
//---------------------------------------------------------

struct DirectionsAnchor {
private:
    EngravingItem *direct; // the element containing the direction
    EngravingItem *anchor; // the element it is attached to
    bool start;            // whether it is attached to start or end
    Fraction tick;         // the timestamp

public:
    DirectionsAnchor(EngravingItem *a, bool s, const Fraction &t) {
        direct = 0;
        anchor = a;
        start = s;
        tick = t;
    }

    DirectionsAnchor(const Fraction &t) {
        direct = 0;
        anchor = 0;
        start = true;
        tick = t;
    }

    EngravingItem *get_direct() { return direct; }

    EngravingItem *get_anchor() { return anchor; }

    bool get_start() { return start; }

    Fraction get_tick() { return tick; }

    void set_direct(EngravingItem *d) { direct = d; }
};

//---------------------------------------------------------
// trill handling
//---------------------------------------------------------

// find all trills in this measure and this part

static void find_trills(const Measure *const measure, track_idx_t strack, track_idx_t etrack, TrillHash &trill_start,
                        TrillHash &trill_stop) {
    // loop over all spanners in this measure
    Fraction stick = measure->tick();
    Fraction etick = measure->end_tick();
    for (auto it = measure->score()->spanner().lower_bound(stick.ticks()); it != measure->score()->spanner().upper_bound(etick.ticks());
         ++it) {
        EngravingItem *e = it->second;
        //LOGD("1 trill %p type %d track %d tick %s", e, e->type(), e->track(), muPrintable(e->tick().print()));
        if (e->is_trill() && strack <= e->track() && e->track() < etrack && e->tick() >= measure->tick() &&
            e->tick() < measure->end_tick()) {
            //LOGD("2 trill %p", e);
            // a trill is found starting in this segment, trill end time is known
            // determine notes to write trill start and stop

            const Trill *tr = to_trill(e);
            EngravingItem *elem1 = tr->start_element();
            EngravingItem *elem2 = tr->end_element();

            if (elem1 && elem1->is_chord_rest() && elem2 && elem2->is_chord_rest()) {
                trill_start.insert({to_chord_rest(elem1), tr});
                trill_stop.insert({to_chord_rest(elem2), tr});
            }
        }
    }
}

//---------------------------------------------------------
// helpers for ::calcDivisions
//---------------------------------------------------------

typedef std::set<Fraction> FractionSet;
static FractionSet fractions;

static void add_fraction(const Fraction &len) { fractions.insert(len.reduced()); }

//---------------------------------------------------------
//   calcDivMoveToTick
//---------------------------------------------------------

void ExportMusicXml::calc_div_move_to_tick(const Fraction &t, const Fraction &stretch) {
    Fraction stretched_tick{t + stretch * (tick() - t)};
#ifdef DEBUG_TICK
    LOGD() << "t (target) " << fractionToStdString(t) << " stretch " << fractionToStdString(stretch) << " m_tick (current) "
           << fractionToStdString(tick_) << " stretchedTick " << fractionToStdString(stretched_tick);
#endif
    if (t < tick()) {
#ifdef DEBUG_TICK
        LOGD() << "backup " << fractionToStdString(tick() - t);
#endif
        add_fraction(stretched_tick - t);
    } else if (t > tick()) {
#ifdef DEBUG_TICK
        LOGD() << "forward " << fractionToStdString(t - tick());
#endif
        add_fraction(t - stretched_tick);
    }
    tick() = t;
}

//---------------------------------------------------------
// isTwoNoteTremolo - determine is chord is part of two note tremolo
//---------------------------------------------------------

static bool is_two_note_tremolo(Chord *chord) { return chord->tremolo_two_chord(); }

//---------------------------------------------------------
//  stretch
//---------------------------------------------------------

static Fraction stretch(Score *score, track_idx_t st, Fraction tick) {
    Staff *staff{score->staff(track2staff(st))};
    Fraction res{staff->time_stretch(tick)};
#ifdef DEBUG_TICK
    LOGD() << "track " << st << " tick " << fractionToStdString(tick) << " stretch " << fractionToStdString(res);
#endif
    return res;
}

//---------------------------------------------------------
//  calcDivisions
//---------------------------------------------------------

// Loop over all voices in all staves and determine a suitable value for divisions.
// All parts are taken into account, a global divisions value results.

// Length of time in MusicXML is expressed in "units", which should allow expressing all time values
// as an integral number of units. Divisions contains the number of units in a quarter note.
// Compute divisions by finding all fractions used to move through the score in MusicXML
// and computing the least common multiple of these fractions numerator and of fraction 1/4.

void ExportMusicXml::calc_divisions() {
    // init
    fractions.clear();

    const std::vector<Part *> &il = score_->parts();

    for (size_t idx = 0; idx < il.size(); ++idx) {
        Part *part = il.at(idx);
        tick_ = {0, 1};

        size_t staves = part->nstaves();
        track_idx_t strack = score_->staff_idx(part) * ::mu::engraving::voices;
        track_idx_t etrack = strack + staves * ::mu::engraving::voices;

        for (MeasureBase *mb = score_->measures()->first(); mb; mb = mb->next()) {
            if (!mb->is_measure()) {
                continue;
            }
            Measure *m = (Measure *) mb;

            for (track_idx_t st = strack; st < etrack; ++st) {
                for (Segment *seg = m->first(); seg; seg = seg->next()) {
                    for (const EngravingItem *e : seg->annotations()) {
                        if (e->track() == st && e->is_figured_bass()) {
                            const FiguredBass *fb = to_figured_bass(e);
#ifdef DEBUG_TICK
                            LOGD() << "figuredbass tick " << fractionToStdString(fb->tick()) << " tickLen "
                                   << fractionToStdString(fb->ticks());
#endif
                            add_fraction(stretch(score_, st, m->tick()) * fb->ticks());
                        }
                    }

                    EngravingItem *el = seg->element(st);
                    if (!el) {
                        continue;
                    }

                    // must ignore start repeat to prevent spurious backup/forward
                    if (el->is_bar_line() && to_bar_line(el)->bar_line_type() == BarLineType::START_REPEAT) {
                        continue;
                    }

                    if (tick_ != seg->tick()) {
                        calc_div_move_to_tick(seg->tick(), stretch(score_, st, m->tick()));
                    }

                    if (el->is_chord_rest()) {
                        Fraction l = to_chord_rest(el)->actual_ticks();
                        if (el->is_chord()) {
                            if (is_two_note_tremolo(to_chord(el))) {
                                l = l * Fraction(1, 2);
                            }
                        }
#ifdef DEBUG_TICK
                        LOGD() << "chordrest tick " << fractionToStdString(el->tick()) << " tickLen"
                               << durElemTicksToStdString(*to_chord_rest(el));
#endif
                        add_fraction(stretch(score_, st, m->tick()) * l);
                        tick_ += l;
                    }
                }
            }
            // move to end of measure (in case of incomplete last voice)
            calc_div_move_to_tick(m->end_tick(), stretch(score_, etrack - 1, m->tick()));
        }
    }

    // compute divisions
    int divisions{4}; // ensure divisions > 0 for half and whole note
    for (auto f : fractions) {
        divisions = std::lcm(divisions, f.denominator());
    }
    divisions /= 4;

#ifdef DEBUG_TICK
    LOGD("divisions %d", divisions);
#endif
    div_ = divisions;
}

//---------------------------------------------------------
//   writePageFormat
//---------------------------------------------------------

static void write_page_format(const MStyle &s, XmlWriter &xml, double conversion) {
    xml.start_element("page-layout");

    xml.tag("page-height", s.style_d(Sid::PAGE_HEIGHT) * conversion);
    xml.tag("page-width", s.style_d(Sid::PAGE_WIDTH) * conversion);

    String type(u"both");
    if (s.style_b(Sid::PAGE_TWOSIDED)) {
        type = u"even";
        xml.start_element("page-margins", {{"type", type}});
        xml.tag("left-margin", s.style_d(Sid::PAGE_EVEN_LEFT_MARGIN) * conversion);
        xml.tag("right-margin", s.style_d(Sid::PAGE_ODD_LEFT_MARGIN) * conversion);
        xml.tag("top-margin", s.style_d(Sid::PAGE_EVEN_TOP_MARGIN) * conversion);
        xml.tag("bottom-margin", s.style_d(Sid::PAGE_EVEN_BOTTOM_MARGIN) * conversion);
        xml.end_element();
        type = u"odd";
    }
    xml.start_element("page-margins", {{"type", type}});
    xml.tag("left-margin", s.style_d(Sid::PAGE_ODD_LEFT_MARGIN) * conversion);
    xml.tag("right-margin", s.style_d(Sid::PAGE_EVEN_LEFT_MARGIN) * conversion);
    xml.tag("top-margin", s.style_d(Sid::PAGE_ODD_TOP_MARGIN) * conversion);
    xml.tag("bottom-margin", s.style_d(Sid::PAGE_ODD_BOTTOM_MARGIN) * conversion);
    xml.end_element();

    xml.end_element();
}

//---------------------------------------------------------
//   defaults
//---------------------------------------------------------

// _spatium = DPMM * (millimeter * 10.0 / tenths);

static void defaults(XmlWriter &xml, const MStyle &s, double &millimeters, const int &tenths) {
    xml.start_element("defaults");
    {
        xml.start_element("scaling");
        xml.tag("millimeters", millimeters);
        xml.tag("tenths", tenths);
        xml.end_element();
    }

    if (s.style_b(Sid::CONCERT_PITCH)) {
        xml.tag("concert-score");
    }

    write_page_format(s, xml, ::mu::engraving::inch / millimeters * tenths);

    // when exporting only manual or no breaks, system-distance is not written at all
    if (s.style_b(Sid::DIVIDER_LEFT) || s.style_b(Sid::DIVIDER_RIGHT)) {
        xml.start_element("system-layout");
        xml.start_element("system-dividers");
        if (s.style_b(Sid::DIVIDER_LEFT)) {
            xml.tag("left-divider", {{"print-object", "yes"},
                                     {"relative-x", s.style_s(Sid::DIVIDER_LEFT_X).val() * 10},
                                     {"relative-y", s.style_s(Sid::DIVIDER_LEFT_Y).val() * 10}});
        } else {
            xml.tag("left-divider", {{"print-object", "no"}});
        }
        if (s.style_b(Sid::DIVIDER_RIGHT)) {
            xml.tag("right-divider", {{"print-object", "yes"},
                                      {"relative-x", s.style_s(Sid::DIVIDER_RIGHT_X).val() * 10},
                                      {"relative-y", s.style_s(Sid::DIVIDER_RIGHT_Y).val() * 10}});
        } else {
            xml.tag("right-divider", {{"print-object", "no"}});
        }
        xml.end_element();
        xml.end_element();
    }

    {
        xml.start_element("appearance");
        // line width values in tenth
        xml.tag("line-width", {{"type", "light barline"}}, s.style_s(Sid::BAR_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "heavy barline"}}, s.style_s(Sid::END_BAR_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "beam"}}, s.style_s(Sid::BEAM_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "bracket"}}, s.style_s(Sid::BRACKET_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "dashes"}}, s.style_s(Sid::LYRICS_DASH_LINE_THICKNESS).val() * 10);
        xml.tag("line-width", {{"type", "enclosure"}}, s.style_s(Sid::STAFF_TEXT_FRAME_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "ending"}}, s.style_s(Sid::VOLTA_LINE_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "extend"}}, s.style_s(Sid::LYRICS_LINE_THICKNESS).val() * 10);
        xml.tag("line-width", {{"type", "leger"}}, s.style_s(Sid::LEDGER_LINE_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "pedal"}}, s.style_s(Sid::PEDAL_LINE_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "octave shift"}}, s.style_s(Sid::OTTAVA_LINE_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "slur middle"}}, s.style_s(Sid::SLUR_MID_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "slur tip"}}, s.style_s(Sid::SLUR_END_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "staff"}}, s.style_s(Sid::STAFF_LINE_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "stem"}}, s.style_s(Sid::STEM_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "tie middle"}}, s.style_s(Sid::TIE_MID_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "tie tip"}}, s.style_s(Sid::TIE_END_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "tuplet bracket"}}, s.style_s(Sid::TUPLET_BRACKET_WIDTH).val() * 10);
        xml.tag("line-width", {{"type", "wedge"}}, s.style_s(Sid::HAIRPIN_LINE_WIDTH).val() * 10);
        // note size values in percent
        xml.tag("note-size", {{"type", "cue"}}, s.style_d(Sid::SMALL_NOTE_MAG) * 100);
        xml.tag("note-size", {{"type", "grace"}}, s.style_d(Sid::GRACE_NOTE_MAG) * 100);
        xml.tag("note-size", {{"type", "grace-cue"}}, s.style_d(Sid::GRACE_NOTE_MAG) * s.style_d(Sid::SMALL_NOTE_MAG) * 100);
        xml.end_element();
    }

    // font defaults
    // as MuseScore supports dozens of different styles, while MusicXML only has defaults
    // for music, words and lyrics, use Tid STAFF (typically used for words)
    // and LYRIC1 to get MusicXML defaults

    xml.tag("music-font", {{"font-family", s.style_st(Sid::MUSICAL_SYMBOL_FONT)}});
    xml.tag("word-font", {{"font-family", s.style_st(Sid::STAFF_TEXT_FONT_FACE)}, {"font-size", s.style_d(Sid::STAFF_TEXT_FONT_SIZE)}});
    xml.tag("lyric-font", {{"font-family", s.style_st(Sid::LYRICS_ODD_FONT_FACE)}, {"font-size", s.style_d(Sid::LYRICS_ODD_FONT_SIZE)}});
    xml.end_element();
}

//---------------------------------------------------------
//   formatForWords
//---------------------------------------------------------

static CharFormat format_for_words(const MStyle &s) {
    CharFormat def_fmt;
    def_fmt.set_font_family(s.style_st(Sid::STAFF_TEXT_FONT_FACE));
    def_fmt.set_font_size(s.style_d(Sid::STAFF_TEXT_FONT_SIZE));
    return def_fmt;
}

//---------------------------------------------------------
//   creditWords
//---------------------------------------------------------

static void credit_words(XmlWriter &xml, const MStyle &s, const page_idx_t page_nr, const double x, const double y, const String &just,
                         const String &val, const std::list<TextFragment> &words, const String &credit_type) {
    // prevent incorrect MusicXML for empty text
    if (words.empty()) {
        return;
    }

    const String mtf = s.style_st(Sid::MUSICAL_TEXT_FONT);
    const CharFormat def_fmt = format_for_words(s);

    // export formatted
    xml.start_element("credit", {{"page", page_nr}});
    if (!credit_type.empty()) {
        xml.tag("credit-type", credit_type);
    }
    String attr = String(u" default-x=\"%1\"").arg(String::number(x, 2));
    attr += String(u" default-y=\"%1\"").arg(String::number(y, 2));
    attr += u" justify=\"" + just + u"\"";
    attr += u" valign=\"" + val + u"\"";
    MScoreTextToMusicXml mttm(u"credit-words", attr, def_fmt, mtf);
    mttm.write_text_fragments(words, xml);
    xml.end_element();
}

//---------------------------------------------------------
//   parentHeight
//---------------------------------------------------------

static double parent_height(const EngravingItem *element) {
    const EngravingItem *parent = element->layout_parent();

    if (!parent) {
        return 0;
    }

    if (parent->is_v_box()) {
        return parent->height();
    }

    return 0;
}

//---------------------------------------------------------
//   tidToCreditType
//---------------------------------------------------------

static String tid_to_credit_type(const TextStyleType tid) {
    String res;
    switch (tid) {
        case TextStyleType::COMPOSER: res = u"composer"; break;
        case TextStyleType::LYRICIST: res = u"lyricist"; break;
        case TextStyleType::SUBTITLE: res = u"subtitle"; break;
        case TextStyleType::TITLE: res = u"title"; break;
        default: break;
    }
    return res;
}

//---------------------------------------------------------
//   textAsCreditWords
//---------------------------------------------------------

// Refactor suggestion: make getTenthsFromInches static instead of ExportMusicXml member function

static void text_as_credit_words(const ExportMusicXml *const exp_mxml, XmlWriter &xml, const MStyle &s, const int page_nr,
                                 const Text *const text) {
    // determine page formatting
    const double h = exp_mxml->get_tenths_from_inches(s.style_d(Sid::PAGE_HEIGHT));
    const double w = exp_mxml->get_tenths_from_inches(s.style_d(Sid::PAGE_WIDTH));
    const double lm = exp_mxml->get_tenths_from_inches(s.style_d(Sid::PAGE_ODD_LEFT_MARGIN));
    const double rm = exp_mxml->get_tenths_from_inches(s.style_d(Sid::PAGE_EVEN_LEFT_MARGIN));
    const double ph = exp_mxml->get_tenths_from_dots(parent_height(text));

    double tx = w / 2;
    double ty = h - exp_mxml->get_tenths_from_dots(text->page_pos().y());

    Align al = text->align();
    String just;
    String val;

    if (al == AlignH::RIGHT) {
        just = u"right";
        tx = w - rm;
    } else if (al == AlignH::HCENTER) {
        just = u"center";
        // tx already set correctly
    } else {
        just = u"left";
        tx = lm;
    }

    if (al == AlignV::BOTTOM) {
        val = u"bottom";
        ty -= ph;
    } else if (al == AlignV::VCENTER) {
        val = u"middle";
        ty -= ph / 2;
    } else if (al == AlignV::BASELINE) {
        val = u"baseline";
        ty -= ph / 2;
    } else {
        val = u"top";
        // ty already set correctly
    }

    const String credit_type = tid_to_credit_type(text->text_style_type());

    credit_words(xml, s, page_nr, tx, ty, just, val, text->fragment_list(), credit_type);
}

//---------------------------------------------------------
//   credits
//---------------------------------------------------------

void ExportMusicXml::credits(XmlWriter &xml) {
    // find the vboxes in every page and write their elements as credit-words
    for (const Page *page : score_->pages()) {
        const page_idx_t page_idx = score_->page_idx(page);
        for (const System *system : page->systems()) {
            for (const MeasureBase *mb : system->measures()) {
                if (mb->is_v_box()) {
                    for (const EngravingItem *element : mb->el()) {
                        if (element->is_text()) {
                            const Text *text = to_text(element);
                            text_as_credit_words(this, xml, score_->style(), static_cast<int>(page_idx) + 1, text);
                        }
                    }
                }
            }
        }
    }

    // put copyright at the bottom center of every page
    // note: as the copyright metatag contains plain text, special XML characters must be escaped
    // determine page formatting
    const String rights = score_->meta_tag(u"copyright");
    if (!rights.is_empty()) {
        const MStyle &style = score_->style();
        const double bm = get_tenths_from_inches(style.style_d(Sid::PAGE_ODD_BOTTOM_MARGIN));
        const double w = get_tenths_from_inches(style.style_d(Sid::PAGE_WIDTH));
        /*
        const double h  = getTenthsFromInches(_score->styleD(Sid::pageHeight));
        const double lm = getTenthsFromInches(_score->styleD(Sid::pageOddLeftMargin));
        const double rm = getTenthsFromInches(_score->styleD(Sid::pageEvenLeftMargin));
        const double tm = getTenthsFromInches(_score->styleD(Sid::pageOddTopMargin));
        LOGD("page h=%g w=%g lm=%g rm=%g tm=%g bm=%g", h, w, lm, rm, tm, bm);
        */
        TextFragment f(XmlWriter::xml_string(rights));
        f.change_format(FormatId::FONT_FAMILY, style.style_st(Sid::FOOTER_FONT_FACE));
        f.change_format(FormatId::FONT_SIZE, style.style_d(Sid::FOOTER_FONT_SIZE));
        std::list<TextFragment> list;
        list.push_back(f);
        for (page_idx_t page_idx = 0; page_idx < score_->npages(); ++page_idx) {
            credit_words(xml, style, page_idx + 1, w / 2, bm, u"center", u"bottom", list, u"rights");
        }
    }
}

//---------------------------------------------------------
//   midipitch2xml
//---------------------------------------------------------

static int alter_tab[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
static char16_t note_tab[12] = {u'C', u'C', u'D', u'D', u'E', u'F', u'F', u'G', u'G', u'A', u'A', u'B'};

static void midipitch2xml(int pitch, char16_t &c, int &alter, int &octave) {
    // 60 = C 4
    c = note_tab[pitch % 12];
    alter = alter_tab[pitch % 12];
    octave = pitch / 12 - 1;
    //LOGD("midipitch2xml(pitch %d) step %c, alter %d, octave %d", pitch, c, alter, octave);
}

//---------------------------------------------------------
//   pitch2xml
//---------------------------------------------------------

// TODO validation

static void pitch2xml(const Note *note, String &s, int &alter, int &octave) {
    const Staff *st = note->staff();
    const Fraction tick = note->tick();
    const Instrument *instr = st->part()->instrument(tick);
    const Interval intval = note->concert_pitch() ? 0 : instr->transpose();

    s = tpc2step_name(note->tpc());
    alter = tpc2alter_by_key(note->tpc(), Key::C);
    // note that pitch must be converted to concert pitch
    // in order to calculate the correct octave
    octave = (note->pitch() - intval.chromatic - alter) / 12 - 1;

    //
    // HACK:
    // On percussion clefs there is no relationship between
    // note->pitch() and note->line()
    // note->line() is determined by drumMap
    //
    ClefType ct = st->clef(tick);
    if (ct == ClefType::PERC || ct == ClefType::PERC2) {
        alter = 0;
        octave = line2pitch(note->line(), ct, Key::C) / 12 - 1;
    }

    // correct for ottava lines
    int ottava = 0;
    switch (note->ppitch() - note->pitch()) {
        case 36: ottava = 3; break;
        case 24: ottava = 2; break;
        case 12: ottava = 1; break;
        case 0: ottava = 0; break;
        case -12: ottava = -1; break;
        case -24: ottava = -2; break;
        case -36: ottava = -3; break;
        default: LOGD("pitch2xml() tick=%d pitch()=%d ppitch()=%d", tick.ticks(), note->pitch(), note->ppitch());
    }
    octave += ottava;

    //LOGD("pitch2xml(pitch %d, tpc %d, ottava %d clef %hhd) step    %s, alter    %d, octave    %d",
    //       note->pitch(), note->tpc(), ottava, clef, muPrintable(s), alter, octave);
}

// unpitch2xml -- calculate display-step and display-octave for an unpitched note
// note:
// even though this produces the correct step/octave according to Recordare's tutorial
// Finale Notepad 2012 does not import a three line staff with percussion clef correctly
// Same goes for Sibelius 6 in case of three or five line staff with percussion clef

static void unpitch2xml(const Note *note, String &s, int &octave) {
    static char16_t table1[] = u"FEDCBAG";

    Fraction tick = note->chord()->tick();
    Staff *st = note->staff();
    ClefType ct = st->clef(tick);

    int note_line = note->line();
    if (note_line == ::mu::engraving::invalid_line) {
        // Invisible staves are skipped by ModifyDom::cmdUpdateNotes(), so a percussion note on
        // one may never have had its display line computed; resolve it from the drumset instead
        const Drumset *drumset = st->part()->instrument(tick)->drumset();
        note_line = (drumset && drumset->is_valid(note->pitch())) ? drumset->line(note->pitch()) : 0;
    }

    // offset in lines between staff with current clef and with G clef
    int clef_offset = ClefInfo::pitch_offset(ct) - ClefInfo::pitch_offset(ClefType::G);
    // line note would be on on a five line staff with G clef
    // note top line is line 0, bottom line is line 8
    int line5g = note_line - clef_offset;
    // in MusicXML with percussion clef, step and octave are determined as if G clef is used
    // when stafflines is not equal to five, in MusicXML the bottom line is still E4.
    // in MuseScore assumes line 0 is F5
    // MS line numbers (top to bottom) plus correction to get lowest line at E4 (line 8)
    // 1 line staff: 0             -> correction 8
    // 3 line staff: 2, 4, 6       -> correction 2
    // 5 line staff: 0, 2, 4, 6, 8 -> correction 0
    // TODO handle other # staff lines ?
    if (st->lines(Fraction(0, 1)) == 1) {
        line5g += 8;
    }
    if (st->lines(Fraction(0, 1)) == 3) {
        line5g += 2;
    }
    // index in table1 to get step
    int step_idx = (line5g + 700) % 7;
    // get step
    s = Char(table1[step_idx]);
    // calculate octave, offset "3" correcting for the fact that an octave starts
    // with C instead of F
    octave = (3 - line5g + 700) / 7 + 5 - 100;
    // LOGD("ExportMusicXml::unpitch2xml(%p) clef %d clef.po %d clefOffset %d staff.lines %d note.line %d line5g %d step %c oct %d",
    //        note, ct, clefTable[ct].pitchOffset, clefOffset, st->lines(), note->line(), line5g, step, octave);
}

//---------------------------------------------------------
//   tick2xml
//    set type + dots depending on tick len
//---------------------------------------------------------

static String tick2xml(const Fraction &ticks, int *dots) {
    TDuration t(ticks);
    *dots = t.dots();
    if (ticks == Fraction(0, 1)) {
        t.set_type(DurationType::V_MEASURE);
        *dots = 0;
    }
    return String::from_ascii(TConv::to_xml(t.type()).ascii());
}

//---------------------------------------------------------
//   findVolta -- find volta starting in measure m
//---------------------------------------------------------

static Volta *find_volta(const Measure *const m, bool left, const track_idx_t track) {
    Fraction stick = m->tick();
    Fraction etick = m->end_tick();
    auto spanners = m->score()->spanner_map().find_overlapping(stick.ticks(), etick.ticks());
    for (auto i : spanners) {
        Spanner *el = i.value;
        if (!el->is_volta() || track2staff(el->track()) != track2staff(track)) {
            continue;
        }
        if (left && el->tick() == stick) {
            return (Volta *) el;
        }
        if (!left && el->tick2() == etick) {
            return (Volta *) el;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   ending
//---------------------------------------------------------

static void ending(XmlWriter &xml, Volta *v, bool left) {
    String number;
    String type;
    for (int i : v->endings()) {
        if (!number.is_empty()) {
            number += u", ";
        }
        number += String::number(i);
    }
    if (left) {
        type = u"start";
    } else {
        Volta::Type st = v->volta_type();
        switch (st) {
            case Volta::Type::OPEN: type = u"discontinue"; break;
            case Volta::Type::CLOSED: type = u"stop"; break;
            default: LOGD("unknown volta subtype %d", int(st)); return;
        }
    }
    String volta_xml = String(u"ending number=\"%1\" type=\"%2\"").arg(number, type);
    volta_xml += ExportMusicXml::positioning_attributes(v, left);
    if (left) {
        if (!v->visible()) {
            volta_xml += u" print-object=\"no\"";
        }
        volta_xml += color2xml(v);
        xml.tag_raw(volta_xml, v->text().to_xml_escaped());
    } else {
        xml.tag_raw(volta_xml);
    }
}

//---------------------------------------------------------
//   barlineLeft -- search for and handle barline left
//---------------------------------------------------------

void ExportMusicXml::barline_left(const Measure *const m, const track_idx_t track) {
    bool rs = m->repeat_start();
    Volta *volta = find_volta(m, true, track);
    if (!rs && !volta) {
        return;
    }
    attr_.do_attr(xml_, false);
    xml_.start_element("barline", {{"location", "left"}});
    if (rs) {
        xml_.tag("bar-style", String(u"heavy-light"));
    }
    if (volta) {
        ending(xml_, volta, true);
    }
    if (rs) {
        XmlWriter::Attributes attrs = {{"direction", "forward"}};
        if (score_->style().style_b(Sid::REPEAT_BAR_TIPS)) {
            attrs.push_back({"winged", "curved"});
        }
        xml_.tag("repeat", attrs);
    }
    xml_.end_element();
}

//---------------------------------------------------------
//   shortBarlineStyle -- recognize normal but shorter barline styles
//---------------------------------------------------------

static String short_barline_style(const BarLine *bl) {
    if (bl->bar_line_type() == BarLineType::NORMAL && !bl->span_staff()) {
        if (bl->span_to() < 0) {
            // lowest point of barline above lowest staff line
            if (bl->span_from() < 0) {
                return u"tick"; // highest point of barline above highest staff line
            } else {
                return u"short"; // highest point of barline below highest staff line
            }
        }
    }

    return String();
}

//---------------------------------------------------------
//   normalBarlineStyle -- recognize other barline styles
//---------------------------------------------------------

static String normal_barline_style(const BarLine *bl) {
    const BarLineType bst = bl->bar_line_type();

    switch (bst) {
        case BarLineType::NORMAL: return u"regular";
        case BarLineType::DOUBLE: return u"light-light";
        case BarLineType::REVERSE_END: return u"heavy-light";
        case BarLineType::BROKEN: return u"dashed";
        case BarLineType::DOTTED: return u"dotted";
        case BarLineType::END:
        case BarLineType::END_REPEAT:
        case BarLineType::END_START_REPEAT: return u"light-heavy";
        case BarLineType::HEAVY: return u"heavy";
        case BarLineType::DOUBLE_HEAVY: return u"heavy-heavy";
        default: LOGD("bar subtype %d not supported", int(bst));
    }

    return String();
}

//---------------------------------------------------------
//   barlineMiddle -- handle barline middle
//---------------------------------------------------------

void ExportMusicXml::barline_middle(const BarLine *bl) {
    bool vis = bl->visible();
    String short_style = short_barline_style(bl);
    String normal_style = normal_barline_style(bl);
    String bar_style;
    if (!vis) {
        bar_style = u"none";
    } else if (!short_style.empty()) {
        bar_style = short_style;
    } else {
        bar_style = normal_style;
    }

    if (!bar_style.empty()) {
        xml_.start_element("barline", {{"location", "middle"}});
        xml_.tag("bar-style", bar_style);
        xml_.end_element();
    }
}

//---------------------------------------------------------
//   fermataPosition -  return fermata y position as MusicXML string
//---------------------------------------------------------

String ExportMusicXml::fermata_position(const Fermata *const fermata) {
    String res;

    if (configuration()->export_layout()) {
        constexpr double spatiu_m2_tenths = 10;
        constexpr double epsilon_value = 0.01;
        const double spatium = fermata->spatium();
        const double def_y = -1 * spatiu_m2_tenths * fermata->ldata()->pos().y() / spatium;
        const double rel_y = -1 * spatiu_m2_tenths * fermata->offset().y() / spatium;

        if (std::abs(def_y) >= epsilon_value) {
            res += String(u" default-y=\"%1\"").arg(String::number(def_y, 2));
        }
        if (std::abs(rel_y) >= epsilon_value) {
            res += String(u" relative-y=\"%1\"").arg(String::number(rel_y, 2));
        }
    }

    return res;
}

//---------------------------------------------------------
//   fermata - write a fermata
//---------------------------------------------------------

static void fermata(const Fermata *const a, XmlWriter &xml) {
    String tag_name = u"fermata";
    tag_name += String(u" type=\"%1\"").arg(a->placement() == PlacementV::ABOVE ? u"upright" : u"inverted");
    tag_name += ExportMusicXml::fermata_position(a);
    tag_name += color2xml(a);
    SymId id = a->sym_id();
    if (id == SymId::FERMATA_ABOVE || id == SymId::FERMATA_BELOW) {
        xml.tag_raw(tag_name);
    } else if (id == SymId::FERMATA_SHORT_ABOVE || id == SymId::FERMATA_SHORT_BELOW) {
        xml.tag_raw(tag_name, "angled");
    } else if (id == SymId::FERMATA_LONG_ABOVE || id == SymId::FERMATA_LONG_BELOW) {
        xml.tag_raw(tag_name, "square");
    } else if (id == SymId::FERMATA_VERY_SHORT_ABOVE || id == SymId::FERMATA_VERY_SHORT_BELOW) {
        xml.tag_raw(tag_name, "double-angled");
    } else if (id == SymId::FERMATA_VERY_LONG_ABOVE || id == SymId::FERMATA_VERY_LONG_BELOW) {
        xml.tag_raw(tag_name, "double-square");
    } else if (id == SymId::FERMATA_LONG_HENZE_ABOVE || id == SymId::FERMATA_LONG_HENZE_BELOW) {
        xml.tag_raw(tag_name, "double-dot");
    } else if (id == SymId::FERMATA_SHORT_HENZE_ABOVE || id == SymId::FERMATA_SHORT_HENZE_BELOW) {
        xml.tag_raw(tag_name, "half-curve");
    } else if (id == SymId::CURLEW_SIGN) {
        xml.tag_raw(tag_name, "curlew");
    } else {
        LOGD("unsupported fermata SymId %d", static_cast<int>(id));
    }
}

//---------------------------------------------------------
//   barlineHasFermata -- search for fermata on barline
//---------------------------------------------------------

static bool barline_has_fermata(const BarLine *const barline, const track_idx_t strack, const track_idx_t etrack) // TODO: track
{
    const Segment *seg = barline ? barline->segment() : 0;
    if (seg) {
        for (const EngravingItem *anno : seg->annotations()) {
            if (anno->is_fermata() && strack <= anno->track() && anno->track() < etrack) {
                return true;
            }
        }
    }

    return false;
}

//---------------------------------------------------------
//   writeBarlineFermata -- write fermata on barline
//---------------------------------------------------------

static void write_barline_fermata(const BarLine *const barline, XmlWriter &xml, const track_idx_t strack, const track_idx_t etrack) {
    const Segment *seg = barline ? barline->segment() : 0;
    if (seg) {
        for (const EngravingItem *anno : seg->annotations()) {
            if (anno->is_fermata() && strack <= anno->track() && anno->track() < etrack) {
                fermata(to_fermata(anno), xml);
            }
        }
    }
}

//---------------------------------------------------------
//   barlineRight -- search for and handle barline right
//---------------------------------------------------------

void ExportMusicXml::barline_right(const Measure *const m, const track_idx_t strack, const track_idx_t etrack) {
    const Measure *mm_r1 = m->covering_mm_rest_or_this();                      // the multi measure rest this measure is covered by
    const Measure *mm_r_lst = mm_r1->is_mm_rest() ? mm_r1->mm_rest_last() : 0; // last measure of replaced sequence of empty measures
    // note: use barlinetype as found in multi measure rest for last measure of replaced sequence
    BarLineType bst = m == mm_r_lst ? mm_r1->end_bar_line_type() : m->end_bar_line_type();
    const bool visible = m->end_bar_line_visible();
    String color;

    bool need_bar_style = (bst != BarLineType::NORMAL && bst != BarLineType::START_REPEAT) || !visible;
    Volta *volta = find_volta(m, false, strack);
    // detect short and tick barlines
    String special;
    const BarLine *bl = m->end_bar_line();
    if (bl) {
        color = color2xml(bl);
        if (bst == BarLineType::NORMAL && !bl->span_staff()) {
            if (bl->span_from() == ::mu::engraving::barline_span_tic_k1_from && bl->span_to() == ::mu::engraving::barline_span_tic_k1_to) {
                special = u"tick";
            }
            if (bl->span_from() == ::mu::engraving::barline_span_tic_k2_from && bl->span_to() == ::mu::engraving::barline_span_tic_k2_to) {
                special = u"tick";
            }
            if (bl->span_from() == ::mu::engraving::barline_span_shor_t1_from &&
                bl->span_to() == ::mu::engraving::barline_span_shor_t1_to) {
                special = u"short";
            }
            if (bl->span_from() == ::mu::engraving::barline_span_shor_t2_from &&
                bl->span_to() == ::mu::engraving::barline_span_shor_t2_from) {
                special = u"short";
            }
        }
    }

    // check fermata
    // no need to take mmrest into account, MS does not create mmrests for measure with fermatas
    const bool has_fermata = barline_has_fermata(m->end_bar_line(), strack, etrack);

    if (!need_bar_style && !volta && special.is_empty() && !has_fermata && color.is_empty()) {
        return;
    }

    xml_.start_element("barline", {{"location", "right"}});
    String tag_name = u"bar-style";
    tag_name += color;
    if (need_bar_style) {
        if (!visible) {
            xml_.tag_raw(tag_name, "none");
        } else {
            switch (bst) {
                case BarLineType::DOUBLE: xml_.tag_raw(tag_name, "light-light"); break;
                case BarLineType::REVERSE_END: xml_.tag_raw(tag_name, "heavy-light"); break;
                case BarLineType::BROKEN: xml_.tag_raw(tag_name, "dashed"); break;
                case BarLineType::DOTTED: xml_.tag_raw(tag_name, "dotted"); break;
                case BarLineType::END:
                case BarLineType::END_REPEAT:
                case BarLineType::END_START_REPEAT: xml_.tag_raw(tag_name, "light-heavy"); break;
                case BarLineType::HEAVY: xml_.tag_raw(tag_name, "heavy"); break;
                case BarLineType::DOUBLE_HEAVY: xml_.tag_raw(tag_name, "heavy-heavy"); break;
                default: LOGD("ExportMusicXml::bar(): bar subtype %d not supported", int(bst)); break;
            }
        }
    } else if (!special.is_empty()) {
        xml_.tag_raw(tag_name, special);
    } else if (!color.is_empty()) {
        xml_.tag_raw(tag_name, "regular");
    }

    write_barline_fermata(m->end_bar_line(), xml_, strack, etrack);

    if (volta) {
        ending(xml_, volta, false);
    }

    if (bst == BarLineType::END_REPEAT || bst == BarLineType::END_START_REPEAT) {
        XmlWriter::Attributes attrs = {{"direction", "backward"}};
        if (m->repeat_count() > 2) {
            attrs.push_back({"times", m->repeat_count()});
        }
        if (score_->style().style_b(Sid::REPEAT_BAR_TIPS)) {
            attrs.push_back({"winged", "curved"});
        }
        xml_.tag("repeat", attrs);
    }

    xml_.end_element();
}

//---------------------------------------------------------
//   calculateTimeDeltaInDivisions
//---------------------------------------------------------

static int calculate_time_delta_in_divisions(const Fraction &t1, const Fraction &t2, const int divisions) {
    const Fraction res_as_fraction{(4 * divisions * (t1 - t2))};
    return res_as_fraction.reduced().numerator();
}

//---------------------------------------------------------
//   calculateDurationInDivisions
//---------------------------------------------------------

static int calculate_duration_in_divisions(const Fraction &tick, const int divisions) {
    return calculate_time_delta_in_divisions(tick, Fraction{0, 1}, divisions);
}

//---------------------------------------------------------
//   moveToTick
//---------------------------------------------------------

void ExportMusicXml::move_to_tick(const Fraction &t, const Fraction &stretch) {
    Fraction stretched_tick{t + stretch * (tick() - t)};
#ifdef DEBUG_TICK
    LOGD() << "t (target) " << fractionToStdString(t) << " stretch " << fractionToStdString(stretch) << " tick() (current) "
           << fractionToStdString(tick_) << " stretchedTick " << fractionToStdString(stretched_tick);
#endif
    if (t < tick_) {
#ifdef DEBUG_TICK
        LOGD(" -> backup");
#endif
        attr_.do_attr(xml_, false);
        xml_.start_element("backup");
        xml_.tag("duration", calculate_time_delta_in_divisions(stretched_tick, t, div_));
        xml_.end_element();
    } else if (t > tick_) {
#ifdef DEBUG_TICK
        LOGD(" -> forward");
#endif
        attr_.do_attr(xml_, false);
        xml_.start_element("forward");
        xml_.tag("duration", calculate_time_delta_in_divisions(t, stretched_tick, div_));
        xml_.end_element();
    }
    tick_ = t;
}

void ExportMusicXml::move_to_tick_if_need(const Fraction &t, track_idx_t track, const Fraction &measure_tick) {
    if (tick_ != t) {
        attr_.do_attr(xml_, false);
        move_to_tick(t, stretch(score_, track, measure_tick));
    }
}

//---------------------------------------------------------
//   timesig
//---------------------------------------------------------

void ExportMusicXml::timesig(const TimeSig *tsig, staff_idx_t staff) {
    const TimeSigType st = tsig->time_sig_type();
    const Fraction ts = tsig->sig();
    const int z = ts.numerator();
    const int n = ts.denominator();
    const String ns = tsig->numerator_string();
    const String ds = tsig->denominator_string();

    attr_.do_attr(xml_, true);
    XmlWriter::Attributes attrs;
    if (st == TimeSigType::FOUR_FOUR) {
        attrs = {{"symbol", "common"}};
    } else if (st == TimeSigType::ALLA_BREVE) {
        attrs = {{"symbol", "cut"}};
    } else if (!ns.empty() && ds.empty()) {
        attrs = {{"symbol", "single-number"}};
    }
    if (staff) {
        attrs.push_back({"number", staff});
    }

    if (!tsig->visible() || !tsig->show_on_this_staff() || (tsig->staff() && !tsig->staff()->staff_type(tsig->tick())->gen_timesig())) {
        attrs.emplace_back(std::make_pair("print-object", "no"));
    }

    add_color_attr(tsig, attrs);

    xml_.start_element("time", attrs);

    if (!ns.empty()) {
        xml_.tag("beats", ns);
    } else {
        xml_.tag("beats", z);
    }
    if (!ds.empty()) {
        xml_.tag("beat-type", ds);
    } else {
        xml_.tag("beat-type", n);
    }
    xml_.end_element();
}

//---------------------------------------------------------
//   accSymId2alter
//---------------------------------------------------------

static double acc_sym_id2alter(SymId id) {
    double res = 0;
    switch (id) {
        case SymId::ACCIDENTAL_FLAT: res = -1; break;
        case SymId::ACCIDENTAL_NATURAL: res = 0; break;
        case SymId::ACCIDENTAL_SHARP: res = 1; break;
        case SymId::ACCIDENTAL_DOUBLE_SHARP: res = 2; break;
        case SymId::ACCIDENTAL_DOUBLE_FLAT: res = -2; break;
        case SymId::ACCIDENTAL_TRIPLE_SHARP: res = 3; break;
        case SymId::ACCIDENTAL_TRIPLE_FLAT: res = -3; break;
        case SymId::ACCIDENTAL_NATURAL_FLAT: res = -1; break;
        case SymId::ACCIDENTAL_NATURAL_SHARP: res = 1; break;
        case SymId::ACCIDENTAL_SHARP_SHARP: res = 2; break;

        // Gould arrow quartertone
        case SymId::ACCIDENTAL_QUARTER_TONE_FLAT_ARROW_UP: res = -0.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_DOWN: res = -1.5; break;
        case SymId::ACCIDENTAL_QUARTER_TONE_SHARP_NATURAL_ARROW_UP: res = 0.5; break;
        case SymId::ACCIDENTAL_QUARTER_TONE_FLAT_NATURAL_ARROW_DOWN: res = -0.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_UP: res = 1.5; break;
        case SymId::ACCIDENTAL_QUARTER_TONE_SHARP_ARROW_DOWN: res = 0.5; break;
        case SymId::ACCIDENTAL_FIVE_QUARTER_TONES_SHARP_ARROW_UP: res = 2.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_ARROW_DOWN: res = 1.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ARROW_UP: res = -1.5; break;
        case SymId::ACCIDENTAL_FIVE_QUARTER_TONES_FLAT_ARROW_DOWN: res = -2.5; break;
        case SymId::ACCIDENTAL_ARROW_DOWN: res = -0.5; break;
        case SymId::ACCIDENTAL_ARROW_UP: res = 0.5; break;

        // Stein-Zimmermann
        case SymId::ACCIDENTAL_QUARTER_TONE_FLAT_STEIN: res = -0.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_FLAT_ZIMMERMANN: res = -1.5; break;
        case SymId::ACCIDENTAL_QUARTER_TONE_SHARP_STEIN: res = 0.5; break;
        case SymId::ACCIDENTAL_THREE_QUARTER_TONES_SHARP_STEIN: res = 1.5; break;

        // Arel-Ezgi-Uzdilek (AEU)
        case SymId::ACCIDENTAL_BUYUK_MUCENNEB_FLAT: res = -0.89; break;
        case SymId::ACCIDENTAL_BAKIYE_FLAT: res = -0.44; break;
        case SymId::ACCIDENTAL_KUCUK_MUCENNEB_SHARP: res = 0.56; break;
        case SymId::ACCIDENTAL_BUYUK_MUCENNEB_SHARP: res = 0.89; break;

        // Persian
        case SymId::ACCIDENTAL_SORI: res = 0.33; break;
        case SymId::ACCIDENTAL_KORON: res = -0.67; break;

        // Wyschnegradsky
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY10_TWELFTHS_FLAT: res = -1.67; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY10_TWELFTHS_SHARP: res = 1.67; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY11_TWELFTHS_FLAT: res = -1.83; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY11_TWELFTHS_SHARP: res = 1.83; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY1_TWELFTHS_FLAT: res = -0.17; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY1_TWELFTHS_SHARP: res = 0.17; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY2_TWELFTHS_FLAT: res = -0.33; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY2_TWELFTHS_SHARP: res = 0.33; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY3_TWELFTHS_FLAT: res = -0.50; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY3_TWELFTHS_SHARP: res = 0.50; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY4_TWELFTHS_FLAT: res = -0.67; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY4_TWELFTHS_SHARP: res = 0.67; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY5_TWELFTHS_FLAT: res = -0.83; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY5_TWELFTHS_SHARP: res = 0.83; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY6_TWELFTHS_FLAT: res = -1; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY6_TWELFTHS_SHARP: res = 1; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY7_TWELFTHS_FLAT: res = -1.16; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY7_TWELFTHS_SHARP: res = 1.16; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY8_TWELFTHS_FLAT: res = -1.33; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY8_TWELFTHS_SHARP: res = 1.33; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY9_TWELFTHS_FLAT: res = -1.50; break;
        case SymId::ACCIDENTAL_WYSCHNEGRADSKY9_TWELFTHS_SHARP: res = 1.50; break;

        // the most important (Spartan) Sagittal accidentals
        case SymId::ACC_SAGITTAL5V7_KLEISMA_DOWN: res = -0.06; break;
        case SymId::ACC_SAGITTAL5V7_KLEISMA_UP: res = 0.06; break;
        case SymId::ACC_SAGITTAL5_COMMA_DOWN: res = -0.22; break;
        case SymId::ACC_SAGITTAL5_COMMA_UP: res = 0.22; break;
        case SymId::ACC_SAGITTAL7_COMMA_DOWN: res = -0.27; break;
        case SymId::ACC_SAGITTAL7_COMMA_UP: res = 0.27; break;
        case SymId::ACC_SAGITTAL25_SMALL_DIESIS_DOWN: res = -0.43; break;
        case SymId::ACC_SAGITTAL25_SMALL_DIESIS_UP: res = 0.43; break;
        case SymId::ACC_SAGITTAL35_MEDIUM_DIESIS_DOWN: res = -0.49; break;
        case SymId::ACC_SAGITTAL35_MEDIUM_DIESIS_UP: res = 0.49; break;
        case SymId::ACC_SAGITTAL11_MEDIUM_DIESIS_DOWN: res = -0.53; break;
        case SymId::ACC_SAGITTAL11_MEDIUM_DIESIS_UP: res = 0.53; break;
        case SymId::ACC_SAGITTAL11_LARGE_DIESIS_DOWN: res = -0.60; break;
        case SymId::ACC_SAGITTAL11_LARGE_DIESIS_UP: res = 0.60; break;
        case SymId::ACC_SAGITTAL35_LARGE_DIESIS_DOWN: res = -0.65; break;
        case SymId::ACC_SAGITTAL35_LARGE_DIESIS_UP: res = 0.65; break;
        case SymId::ACC_SAGITTAL_FLAT25_S_UP: res = -0.71; break;
        case SymId::ACC_SAGITTAL_SHARP25_S_DOWN: res = 0.71; break;
        case SymId::ACC_SAGITTAL_FLAT7_C_UP: res = -0.86; break;
        case SymId::ACC_SAGITTAL_SHARP7_C_DOWN: res = 0.86; break;
        case SymId::ACC_SAGITTAL_FLAT5_C_UP: res = -0.92; break;
        case SymId::ACC_SAGITTAL_SHARP5_C_DOWN: res = 0.92; break;
        case SymId::ACC_SAGITTAL_FLAT5V7K_UP: res = -1.08; break;
        case SymId::ACC_SAGITTAL_SHARP5V7K_DOWN: res = 1.08; break;
        case SymId::ACC_SAGITTAL_FLAT: res = -1.14; break;
        case SymId::ACC_SAGITTAL_SHARP: res = 1.14; break;

        // Turkish folk music accidentals
        case SymId::ACCIDENTAL1_COMMA_FLAT: res = -0.22; break;
        case SymId::ACCIDENTAL1_COMMA_SHARP: res = 0.22; break;
        case SymId::ACCIDENTAL2_COMMA_FLAT: res = -0.44; break;
        case SymId::ACCIDENTAL2_COMMA_SHARP: res = 0.44; break;
        case SymId::ACCIDENTAL3_COMMA_FLAT: res = -0.67; break;
        case SymId::ACCIDENTAL3_COMMA_SHARP: res = 0.67; break;
        case SymId::ACCIDENTAL4_COMMA_FLAT: res = -0.89; break;
        case SymId::ACCIDENTAL5_COMMA_SHARP: res = 1.11; break;
        default: LOGD("accSymId2alter: unsupported sym %s", SymNames::name_for_sym_id(id).ascii());
    }
    return res;
}

//---------------------------------------------------------
//   keysig
//---------------------------------------------------------

void ExportMusicXml::keysig(const KeySig *ks, ClefType ct, staff_idx_t staff) {
    static char16_t table2[] = u"CDEFGAB";
    int po = ClefInfo::pitch_offset(ct); // actually 7 * oct + step for topmost staff line
    //LOGD("keysig st %d key %d custom %d ct %hhd st %d", staff, ks->key(), ks->isCustom(), ct, staff);
    //LOGD(" pitch offset clef %d stp %d oct %d ", po, po % 7, po / 7);

    XmlWriter::Attributes attrs;
    if (staff) {
        attrs.emplace_back(std::make_pair("number", staff));
    }
    if (!ks->visible() || (ks->staff() && ks->staff()->is_tab_staff(Fraction(0, 1)))) {
        attrs.emplace_back(std::make_pair("print-object", "no"));
    }
    add_color_attr(ks, attrs);

    attr_.do_attr(xml_, true);
    xml_.start_element("key", attrs);

    //! NOTE It looks like there is some kind of problem here,
    //! layout data should not be used to write to a file or export
    const KeySig::LayoutData *ldata = ks->ldata();
    const std::vector<KeySym> &keysyms = ldata->key_symbols;
    if (ks->is_custom() && !ks->is_atonal() && keysyms.size() > 0) {
        // non-traditional key signature
        // MusicXML order is left-to-right order, while KeySims in keySymbols()
        // are in insertion order -> sorting required

        // first put the KeySyms in a map
        std::map<double, KeySym> map;
        for (const KeySym &ksym : keysyms) {
            map.insert({ksym.x_pos.val(), ksym});
        }
        // then write them (automatically sorted on key)
        for (const auto &[_, ksym] : map) {
            int step = (po - ksym.line) % 7;
            //LOGD(" keysym sym %d -> line %d step %d", ksym.sym, ksym.line, step);
            xml_.tag("key-step", String(Char(table2[step])));
            xml_.tag("key-alter", acc_sym_id2alter(ksym.sym));
            XmlWriter::Attributes accidental_attrs;
            String s = acc_sym_id2_music_xml_string(ksym.sym);
            if (s == u"other") {
                accidental_attrs = {{"smufl", acc_sym_id2_smufl_music_xml_string(ksym.sym)}};
            }
            xml_.tag("key-accidental", accidental_attrs, s);
        }
    } else {
        // traditional key signature
        xml_.tag("fifths", static_cast<int>(ks->key()));
        if (ks->mode() != KeyMode::UNKNOWN) {
            xml_.tag("mode", String::from_ascii(TConv::to_xml(ks->mode()).ascii()));
        }
    }
    xml_.end_element();
}

//---------------------------------------------------------
//   clef
//---------------------------------------------------------
struct MusicXmlClefInfo {
    ClefType type;
    const char *sign;
    int oct_chng;
};

// table must be in sync with enum ClefType in types.h
static const std::vector<MusicXmlClefInfo> clef_infos = {
    {ClefType::G, "G", 0},
    {ClefType::G15_MB, "G", -2},
    {ClefType::G8_VB, "G", -1},
    {ClefType::G8_VA, "G", 1},
    {ClefType::G15_MA, "G", 2},
    {ClefType::G8_VB_O, "G", -1},
    {ClefType::G8_VB_P, "G", 0},
    {ClefType::G_1, "G", 0},

    {ClefType::C1, "C", 0},
    {ClefType::C2, "C", 0},
    {ClefType::C3, "C", 0},
    {ClefType::C4, "C", 0},
    {ClefType::C5, "C", 0},

    {ClefType::C_19C, "G", 0},

    {ClefType::C1_F18C, "C", 0},
    {ClefType::C3_F18C, "C", 0},
    {ClefType::C4_F18C, "C", 0},
    {ClefType::C1_F20C, "C", 0},
    {ClefType::C3_F20C, "C", 0},
    {ClefType::C4_F20C, "C", 0},

    {ClefType::F, "F", 0},
    {ClefType::F15_MB, "F", -2},
    {ClefType::F8_VB, "F", -1},
    {ClefType::F_8VA, "F", 1},
    {ClefType::F_15MA, "F", 2},
    {ClefType::F_B, "F", 0},
    {ClefType::F_C, "F", 0},
    {ClefType::F_F18C, "F", 0},
    {ClefType::F_19C, "F", 0},

    {ClefType::PERC, "percussion", 0},
    {ClefType::PERC2, "percussion", 0},

    {ClefType::TAB, "TAB", 0},
    {ClefType::TAB4, "TAB", 0},
    {ClefType::TAB_SERIF, "TAB", 0},
    {ClefType::TAB4_SERIF, "TAB", 0},

    {ClefType::C4_8VB, "C", -1},
    {ClefType::G8_VB_C, "G", -1},
};

static const MusicXmlClefInfo find_clef_info_by_type(const ClefType &v) {
    auto it = std::find_if(::mu::iex::musicxml::clef_infos.cbegin(), ::mu::iex::musicxml::clef_infos.cend(),
                           [v](const MusicXmlClefInfo &i) { return i.type == v; });

    IF_ASSERT_FAILED(it != ::mu::iex::musicxml::clef_infos.cend()) {
        static MusicXmlClefInfo dummy_value;
        return dummy_value;
    }
    return *it;
}

void ExportMusicXml::clef(staff_idx_t staff, const ClefType ct, const String &extra_attributes) {
    clefDebug("ExportMusicXml::clef(staff %zu, clef %hhd)", staff, ct);

    String tag_name = u"clef";
    if (staff) {
        tag_name += String(u" number=\"%1\"").arg(static_cast<int>(staff));
    }
    tag_name += extra_attributes;
    attr_.do_attr(xml_, true);
    xml_.start_element_raw(tag_name);

    MusicXmlClefInfo info = find_clef_info_by_type(ct);

    int line = ClefInfo::line(ct);
    xml_.tag("sign", info.sign);
    if ((std::string_view(info.sign) != "percussion") && (std::string_view(info.sign) != "TAB")) {
        xml_.tag("line", line);
    }
    if (info.oct_chng) {
        xml_.tag("clef-octave-change", info.oct_chng);
    }
    xml_.end_element();
}

//---------------------------------------------------------
//   tupletNesting
//---------------------------------------------------------

/*
 * determine the tuplet nesting level for cr
 * 0 = not part of tuplet
 * 1 = part of a single tuplet
 * 2 = part of two nested tuplets
 * etc.
 */

static int tuplet_nesting(const ChordRest *const cr) {
    const DurationElement *el{cr->tuplet()};
    int nesting{0};
    while (el) {
        nesting++;
        el = el->tuplet();
    }
    return nesting;
}

//---------------------------------------------------------
//   isSimpleTuplet
//---------------------------------------------------------

/*
 * determine if t is simple, i.e. all its children are chords or rests
 */

static bool is_simple_tuplet(const Tuplet *const t) {
    if (t->tuplet()) {
        return false;
    }
    for (const EngravingItem *el : t->elements()) {
        if (!el->is_chord_rest()) {
            return false;
        }
    }
    return true;
}

//---------------------------------------------------------
//   isTupletStart
//---------------------------------------------------------

/*
 * determine if t is the starting element of a tuplet
 */

static bool is_tuplet_start(const DurationElement *const el) {
    const Tuplet *t = el->tuplet();
    if (!t) {
        return false;
    }
    return el == t->elements().front();
}

//---------------------------------------------------------
//   isTupletStop
//---------------------------------------------------------

/*
 * determine if t is the stopping element of a tuplet
 */

static bool is_tuplet_stop(const DurationElement *const el) {
    const Tuplet *t = el->tuplet();
    if (!t) {
        return false;
    }
    return el == t->elements().back();
}

//---------------------------------------------------------
//   startTupletAtLevel
//---------------------------------------------------------

/*
 * return the tuplet starting at tuplet nesting level, if any
 */

static const Tuplet *start_tuplet_at_level(const DurationElement *const cr, const int level) {
    const DurationElement *el{cr};
    if (!el->tuplet()) {
        return nullptr;
    }
    for (int i = 0; i < level; ++i) {
        if (!is_tuplet_start(el)) {
            return nullptr;
        }
        el = el->tuplet();
    }
    return to_tuplet(el);
}

//---------------------------------------------------------
//   stopTupletAtLevel
//---------------------------------------------------------

/*
 * return the tuplet stopping at tuplet nesting level, if any
 */

static const Tuplet *stop_tuplet_at_level(const DurationElement *const cr, const int level) {
    const DurationElement *el{cr};
    if (!el->tuplet()) {
        return nullptr;
    }
    for (int i = 0; i < level; ++i) {
        if (!is_tuplet_stop(el)) {
            return nullptr;
        }
        el = el->tuplet();
    }
    return to_tuplet(el);
}

//---------------------------------------------------------
//   tupletTypeAndDots
//---------------------------------------------------------

static void tuplet_type_and_dots(const String &type, const int dots, XmlWriter &xml) {
    xml.tag("tuplet-type", type);
    for (int i = 0; i < dots; ++i) {
        xml.tag("tuplet-dot");
    }
}

//---------------------------------------------------------
//   tupletActualAndNormal
//---------------------------------------------------------

static void tuplet_actual_and_normal(const Tuplet *const t, XmlWriter &xml) {
    xml.start_element("tuplet-actual");
    XmlWriter::Attributes t_num_attrs;
    add_color_attr(t, t_num_attrs);
    xml.tag("tuplet-number", t_num_attrs, t->ratio().numerator());
    int dots{0};
    const String s = tick2xml(t->base_len().ticks(), &dots);
    tuplet_type_and_dots(s, dots, xml);
    xml.end_element();
    xml.start_element("tuplet-normal");
    xml.tag("tuplet-number", t_num_attrs, t->ratio().denominator());
    tuplet_type_and_dots(s, dots, xml);
    xml.end_element();
}

//---------------------------------------------------------
//   tupletStart
//---------------------------------------------------------

// LVIFIX: add placement to tuplet support
// <notations>
//   <tuplet type="start" placement="above" bracket="no"/>
// </notations>

static void tuplet_start(const Tuplet *const t, const int number, const bool need_actual_and_normal, Notations &notations, XmlWriter &xml) {
    notations.tag(xml, t);
    String tuplet_tag = u"tuplet type=\"start\"";
    if (!is_simple_tuplet(t)) {
        tuplet_tag += String(u" number=\"%1\"").arg(number);
    }
    tuplet_tag += u" bracket=";
    tuplet_tag += t->has_bracket() ? u"\"yes\"" : u"\"no\"";
    if (t->number_type() == TupletNumberType::SHOW_RELATION) {
        tuplet_tag += u" show-number=\"both\"";
    } else if (t->number_type() == TupletNumberType::NO_TEXT) {
        tuplet_tag += u" show-number=\"none\"";
    }
    if (t->direction() != engraving::DirectionV::AUTO) {
        if (t->direction() == engraving::DirectionV::UP) {
            tuplet_tag += u" placement=\"above\"";
        } else if (t->direction() == engraving::DirectionV::DOWN) {
            tuplet_tag += u" placement=\"below\"";
        }
    }
    if (need_actual_and_normal) {
        xml.start_element_raw(tuplet_tag);
        tuplet_actual_and_normal(t, xml);
        xml.end_element();
    } else {
        xml.tag_raw(tuplet_tag);
    }
}

//---------------------------------------------------------
//   tupletStop
//---------------------------------------------------------

static void tuplet_stop(const Tuplet *const t, const int number, Notations &notations, XmlWriter &xml) {
    notations.tag(xml, t);
    XmlWriter::Attributes tuplet_attrs = {{"type", "stop"}};
    if (!is_simple_tuplet(t)) {
        tuplet_attrs.emplace_back(std::make_pair("number", number));
    }
    xml.tag("tuplet", tuplet_attrs);
}

//---------------------------------------------------------
//   tupletStartStop
//---------------------------------------------------------

static void tuplet_start_stop(ChordRest *cr, Notations &notations, XmlWriter &xml) {
    const int nesting = tuplet_nesting(cr);
    bool do_actual_and_normal = (nesting > 1);
    if (cr->is_chord() && is_two_note_tremolo(to_chord(cr))) {
        do_actual_and_normal = true;
    }
    for (int level = nesting - 1; level >= 0; --level) {
        const Tuplet *start_tuplet = start_tuplet_at_level(cr, level + 1);
        if (start_tuplet) {
            tuplet_start(start_tuplet, nesting - level, do_actual_and_normal, notations, xml);
        }
        const Tuplet *stop_tuplet = stop_tuplet_at_level(cr, level + 1);
        if (stop_tuplet) {
            tuplet_stop(stop_tuplet, nesting - level, notations, xml);
        }
    }
}

//---------------------------------------------------------
//   findTrill -- get index of trill in trill table
//   return -1 if not found
//---------------------------------------------------------

int ExportMusicXml::find_trill(const Trill *tr) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (trills_[i] == tr) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   writeAccidental
//---------------------------------------------------------

static void write_accidental(XmlWriter &xml, const String &tag_name, const Accidental *const acc) {
    if (acc) {
        String s = accidental_type2_music_xml_string(acc->accidental_type());
        if (!s.empty()) {
            XmlWriter::Attributes attrs;
            if (s == "other") {
                attrs = {{"smufl", accidental_type2_smufl_music_xml_string(acc->accidental_type())}};
            }
            String tag = tag_name;
            if (acc->bracket() == AccidentalBracket::BRACKET) {
                if (acc->role() == AccidentalRole::USER) {
                    attrs.emplace_back(std::make_pair("editorial", "yes"));
                }
                attrs.emplace_back(std::make_pair("bracket", "yes"));
            } else if (acc->bracket() == AccidentalBracket::PARENTHESIS) {
                if (acc->role() == AccidentalRole::USER) {
                    attrs.emplace_back(std::make_pair("cautionary", "yes"));
                }
                attrs.emplace_back(std::make_pair("parentheses", "yes"));
            } else if (acc->role() == AccidentalRole::USER) {            // no way to tell "cautionary" from "editorial"
                attrs.emplace_back(std::make_pair("cautionary", "yes")); // so pick one
                attrs.emplace_back(std::make_pair("parentheses", "no")); // but use neither parenthesis nor bracket ;-)
            }
            if (tag_name == "accidental-mark") {
                if (acc->place_above()) {
                    attrs.emplace_back(std::make_pair("placement", "above"));
                } else if (acc->place_below()) {
                    attrs.emplace_back(std::make_pair("placement", "below"));
                }
            }
            if (acc->is_small()) {
                // only set if the accidental is smaller than the notehead
                const bool tiny = acc->note()->is_small() || acc->note()->chord()->is_small();
                attrs.emplace_back(std::make_pair("size", tiny ? "grace-cue" : "cue"));
            }
            add_color_attr(acc, attrs);
            xml.tag(AsciiStringView(tag.to_std_string()), attrs, s);
        }
    }
}

//---------------------------------------------------------
//   writeDisplayName
//---------------------------------------------------------

static void write_display_name(XmlWriter &xml, const String &part_name) {
    // TODO: add text style attributes
    String display_text;
    for (size_t i = 0; i < part_name.size(); ++i) {
        Char ch = part_name.at(i);
        if (ch != u'♭' && ch != u'♯') {
            display_text += ch;
        } else {
            if (!display_text.empty()) {
                xml.tag("display-text", display_text);
            }
            if (ch == u'♭') {
                xml.tag("accidental-text", "flat");
            } else if (ch == u'♯') {
                xml.tag("accidental-text", "sharp");
            }
            display_text.clear();
        }
    }
    if (!display_text.empty()) {
        xml.tag("display-text", display_text);
    }
}

//---------------------------------------------------------
//   wavyLineStart
//---------------------------------------------------------

static void wavy_line_start(const Trill *tr, const int number, Notations &notations, XmlWriter &xml) {
    notations.tag(xml, tr, "ornaments");
    switch (tr->trill_type()) {
        case TrillType::TRILL_LINE: xml.tag_raw(u"trill-mark" + color2xml(tr)); break;
        case TrillType::UPPRALL_LINE: xml.tag_raw(u"other-ornament smufl=\"ornamentBottomLeftConcaveStroke\"" + color2xml(tr)); break;
        case TrillType::DOWNPRALL_LINE: xml.tag_raw(u"other-ornament smufl=\"ornamentLeftVerticalStroke\"" + color2xml(tr)); break;
        case TrillType::PRALLPRALL_LINE:
        default: break;
    }
    String tag_name = u"wavy-line type=\"start\"";
    tag_name += String(u" number=\"%1\"").arg(number + 1);
    tag_name += color2xml(tr);
    tag_name += ExportMusicXml::positioning_attributes(tr, true);
    xml.tag_raw(tag_name);
    write_accidental(xml, u"accidental-mark", tr->accidental());
}

//---------------------------------------------------------
//   wavyLineStop
//---------------------------------------------------------

static void wavy_line_stop(const Trill *tr, const int number, Notations &notations, XmlWriter &xml) {
    notations.tag(xml, tr, "ornaments");
    String trill_xml = String(u"wavy-line type=\"stop\" number=\"%1\"").arg(number + 1);
    trill_xml += ExportMusicXml::positioning_attributes(tr, false);
    xml.tag_raw(trill_xml);
}

//---------------------------------------------------------
//   wavyLineStartStop
//---------------------------------------------------------

void ExportMusicXml::wavy_line_start_stop(const ChordRest *cr, Notations &notations, TrillHash &trill_start, TrillHash &trill_stop) {
    if (muse::contains(trill_start, cr) && muse::contains(trill_stop, cr)) {
        const Trill *tr = trill_start.at(cr);
        int n = find_trill(0);
        if (n >= 0) {
            wavy_line_start(tr, n, notations, xml_);
            wavy_line_stop(tr, n, notations, xml_);
        } else {
            LOGD("too many overlapping trills (cr %p staff %zu tick %d)", cr, cr->staff_idx(), cr->tick().ticks());
        }
    } else {
        if (muse::contains(trill_stop, cr)) {
            const Trill *tr = trill_stop.at(cr);
            int n = find_trill(tr);
            if (n >= 0) {
                // trill stop after trill start
                trills_[n] = 0;
            } else {
                // trill stop before trill start
                n = find_trill(0);
                if (n >= 0) {
                    trills_[n] = tr;
                } else {
                    LOGD("too many overlapping trills (cr %p staff %zu tick %d)", cr, cr->staff_idx(), cr->tick().ticks());
                }
            }
            if (n >= 0) {
                wavy_line_stop(tr, n, notations, xml_);
            }
            muse::remove(trill_stop, cr);
        }
        if (muse::contains(trill_start, cr)) {
            const Trill *tr = trill_start.at(cr);
            int n = find_trill(tr);
            if (n >= 0) {
                LOGD("wavyLineStartStop error");
            } else {
                n = find_trill(0);
                if (n >= 0) {
                    trills_[n] = tr;
                    wavy_line_start(tr, n, notations, xml_);
                } else {
                    LOGD("too many overlapping trills (cr %p staff %zu tick %d)", cr, cr->staff_idx(), cr->tick().ticks());
                }
                muse::remove(trill_start, cr);
            }
        }
    }
}

//---------------------------------------------------------
//   tremoloSingleStartStop
//---------------------------------------------------------

static void tremolo_single_start_stop(Chord *chord, Notations &notations, XmlWriter &xml) {
    TremoloType st = chord->tremolo_type();
    const EngravingItem *tr =
        chord->tremolo_single_chord() ? to_engraving_item(chord->tremolo_single_chord()) : to_engraving_item(chord->tremolo_two_chord());

    if (st != TremoloType::INVALID_TREMOLO) {
        int count = 0;
        String type;

        if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_SINGLE) {
            if (st == TremoloType::BUZZ_ROLL) {
                type = u"unmeasured";
                count = 0;
            } else {
                type = u"single";
                switch (st) {
                    case TremoloType::R8: count = 1; break;
                    case TremoloType::R16: count = 2; break;
                    case TremoloType::R32: count = 3; break;
                    case TremoloType::R64: count = 4; break;
                    case TremoloType::R128: count = 5; break;
                    case TremoloType::R256: count = 6; break;
                    default: LOGD("unknown tremolo single %d", int(st)); break;
                }
            }
        } else if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_FIRST_CHORD) {
            type = u"start";
            switch (st) {
                case TremoloType::C8: count = 1; break;
                case TremoloType::C16: count = 2; break;
                case TremoloType::C32: count = 3; break;
                case TremoloType::C64: count = 4; break;
                case TremoloType::C128: count = 5; break;
                case TremoloType::C256: count = 6; break;
                default: LOGD("unknown tremolo double %d", int(st)); break;
            }
        } else if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_SECOND_CHORD) {
            type = u"stop";
            switch (st) {
                case TremoloType::C8: count = 1; break;
                case TremoloType::C16: count = 2; break;
                case TremoloType::C32: count = 3; break;
                case TremoloType::C64: count = 4; break;
                case TremoloType::C128: count = 5; break;
                case TremoloType::C256: count = 6; break;
                default: LOGD("unknown tremolo double %d", int(st)); break;
            }
        } else {
            LOGD("unknown tremolo subtype %d", int(st));
        }

        if (!type.empty() && ((count > 0 && type != u"unmeasured") || (count == 0 && type == u"unmeasured"))) {
            notations.tag(xml, tr, "ornaments");
            XmlWriter::Attributes attrs = {{"type", type}};
            if (type != u"stop") {
                add_color_attr(tr, attrs);
            }
            xml.tag("tremolo", attrs, count);
        }
    }
}

//---------------------------------------------------------
//   fermatas
//---------------------------------------------------------

static void fermatas(const std::vector<EngravingItem *> &cra, XmlWriter &xml, Notations &notations) {
    for (const EngravingItem *e : cra) {
        if (!e->is_fermata()) {
            continue;
        }
        notations.tag(xml, e);
        fermata(to_fermata(e), xml);
    }
}

//---------------------------------------------------------
//   symIdToArtic
//---------------------------------------------------------

static std::vector<String> sym_id_to_artic(const SymId sid) {
    switch (sid) {
        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW: return {u"accent"};
        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW: return {u"staccato"};
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW: return {u"staccatissimo"};
        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW: return {u"tenuto"};
        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW: return {u"strong-accent"};
        case SymId::ARTIC_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_TENUTO_STACCATO_BELOW: return {u"detached-legato"};
        case SymId::ARTIC_SOFT_ACCENT_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_BELOW: return {u"soft-accent"};
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_BELOW: return {u"soft-accent", u"staccato"};
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_BELOW: return {u"soft-accent", u"tenuto"};
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_BELOW: return {u"soft-accent", u"detached-legato"};
        case SymId::ARTIC_STRESS_ABOVE:
        case SymId::ARTIC_STRESS_BELOW: return {u"stress"};
        case SymId::ARTIC_UNSTRESS_ABOVE:
        case SymId::ARTIC_UNSTRESS_BELOW: return {u"unstress"};
        case SymId::ARTIC_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_ACCENT_STACCATO_BELOW: return {u"accent", u"staccato"};
        case SymId::ARTIC_MARCATO_STACCATO_ABOVE:
        case SymId::ARTIC_MARCATO_STACCATO_BELOW: return {u"strong-accent", u"staccato"};
        case SymId::ARTIC_MARCATO_TENUTO_ABOVE:
        case SymId::ARTIC_MARCATO_TENUTO_BELOW: return {u"strong-accent", u"tenuto"};
        case SymId::ARTIC_TENUTO_ACCENT_ABOVE:
        case SymId::ARTIC_TENUTO_ACCENT_BELOW: return {u"tenuto", u"accent"};
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW: return {u"spiccato"};
        default: return {}; // nothing
    }
}

//---------------------------------------------------------
//   symIdToOrnam
//---------------------------------------------------------

static String sym_id_to_ornam(const SymId sid) {
    switch (sid) {
        case SymId::ORNAMENT_TRILL: return u"trill-mark";
        case SymId::ORNAMENT_TURN: return u"turn";
        case SymId::ORNAMENT_TURN_INVERTED: return u"inverted-turn";
        case SymId::ORNAMENT_TURN_SLASH: return u"turn slash=\"yes\"";
        case SymId::ORNAMENT_TURN_UP: return u"vertical-turn";
        case SymId::ORNAMENT_TURN_UP_S: return u"inverted-vertical-turn";
        case SymId::ORNAMENT_MORDENT: return u"mordent";
        case SymId::ORNAMENT_SHORT_TRILL: return u"inverted-mordent";
        case SymId::ORNAMENT_TREMBLEMENT: return u"inverted-mordent long=\"yes\"";
        case SymId::ORNAMENT_PRALL_MORDENT: return u"mordent long=\"yes\"";
        case SymId::ORNAMENT_UP_PRALL: return u"inverted-mordent long=\"yes\" approach=\"below\"";
        case SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX: return u"inverted-mordent long=\"yes\" approach=\"above\"";
        case SymId::ORNAMENT_UP_MORDENT: return u"mordent long=\"yes\" approach=\"below\"";
        case SymId::ORNAMENT_DOWN_MORDENT: return u"mordent long=\"yes\" approach=\"above\"";
        case SymId::ORNAMENT_PRALL_DOWN: return u"inverted-mordent long=\"yes\" departure=\"below\"";
        case SymId::ORNAMENT_PRALL_UP: return u"inverted-mordent long=\"yes\" departure=\"above\"";
        case SymId::ORNAMENT_LINE_PRALL:
            // MusicXML 3.0 did not distinguish between downprall and lineprall
            return u"inverted-mordent long=\"yes\" approach=\"above\"";
        case SymId::ORNAMENT_HAYDN: return u"haydn";
        case SymId::ORNAMENT_PRECOMP_SLIDE: return u"schleifer";
        default:
            // use other-ornament
            const AsciiStringView name = SymNames::name_for_sym_id(sid);
            return String(u"other-ornament smufl=\"%1\"").arg(String::from_ascii(name.ascii()));
    }
}

//---------------------------------------------------------
//   symIdToTechn
//---------------------------------------------------------

static String sym_id_to_techn(const SymId sid) {
    switch (sid) {
        case SymId::STRINGS_UP_BOW: return u"up-bow";
        case SymId::STRINGS_DOWN_BOW: return u"down-bow";
        case SymId::STRINGS_HARMONIC: return u"harmonic";
        case SymId::STRINGS_THUMB_POSITION:
        case SymId::STRINGS_THUMB_POSITION_TURNED: return u"thumb-position";
        case SymId::DOUBLE_TONGUE_ABOVE:
        case SymId::DOUBLE_TONGUE_BELOW: return u"double-tongue";
        case SymId::TRIPLE_TONGUE_ABOVE:
        case SymId::TRIPLE_TONGUE_BELOW: return u"triple-tongue";
        case SymId::BRASS_MUTE_CLOSED: return u"stopped";
        case SymId::PLUCKED_SNAP_PIZZICATO_ABOVE:
        case SymId::PLUCKED_SNAP_PIZZICATO_BELOW: return u"snap-pizzicato";
        case SymId::GUITAR_LEFT_HAND_TAPPING: return u"tap hand=\"left\"";
        case SymId::GUITAR_RIGHT_HAND_TAPPING: return u"tap hand=\"right\"";
        case SymId::KEYBOARD_PEDAL_HEEL1:
        case SymId::KEYBOARD_PEDAL_HEEL2:
        case SymId::KEYBOARD_PEDAL_HEEL3: return u"heel";
        case SymId::KEYBOARD_PEDAL_TOE1:
        case SymId::KEYBOARD_PEDAL_TOE2: return u"toe";
        case SymId::PLUCKED_WITH_FINGERNAILS: return u"fingernails";
        case SymId::BRASS_BEND: return u"brass-bend";
        case SymId::BRASS_FLIP: return u"brass-flip";
        case SymId::BRASS_SMEAR: return u"smear";
        case SymId::BRASS_MUTE_OPEN: return u"open";
        case SymId::BRASS_MUTE_HALF_CLOSED: return u"half-muted";
        case SymId::BRASS_HARMON_MUTE_CLOSED:
        case SymId::BRASS_HARMON_MUTE_STEM_HALF_LEFT:
        case SymId::BRASS_HARMON_MUTE_STEM_HALF_RIGHT:
        case SymId::BRASS_HARMON_MUTE_STEM_OPEN: return u"harmon-mute";
        case SymId::WIND_CLOSED_HOLE:
        case SymId::WIND_HALF_CLOSED_HOLE1:
        case SymId::WIND_HALF_CLOSED_HOLE2:
        case SymId::WIND_HALF_CLOSED_HOLE3:
        case SymId::WIND_OPEN_HOLE: return u"hole";
        case SymId::GUITAR_GOLPE: return u"golpe";
        case SymId::HANDBELLS_BELLTREE: return u"belltree";
        case SymId::HANDBELLS_DAMP3: return u"damp";
        case SymId::HANDBELLS_ECHO1: return u"echo";
        case SymId::HANDBELLS_GYRO: return u"gyro";
        case SymId::HANDBELLS_HAND_MARTELLATO: return u"hand martellato";
        case SymId::HANDBELLS_MALLET_LFT: return u"mallet lift";
        case SymId::HANDBELLS_MALLET_BELL_SUSPENDED:
            return String(u"stopped smufl=\"%1\"").arg(String::from_ascii(SymNames::name_for_sym_id(sid).ascii()));
        case SymId::HANDBELLS_MALLET_BELL_ON_TABLE: return u"mallet table";
        case SymId::HANDBELLS_MARTELLATO: return u"martellato";
        case SymId::HANDBELLS_MARTELLATO_LIFT: return u"martellato lift";
        case SymId::HANDBELLS_MUTED_MARTELLATO: return u"muted martellato";
        case SymId::HANDBELLS_PLUCK_LIFT: return u"pluck lift";
        case SymId::HANDBELLS_SWING: return u"swing";
        case SymId::GUITAR_CLOSE_PEDAL:
        case SymId::PICT_OPEN_RIM_SHOT:
            return String(u"stopped smufl=\"%1\"").arg(String::from_ascii(SymNames::name_for_sym_id(sid).ascii()));
        case SymId::GUITAR_HALF_OPEN_PEDAL:
        case SymId::PICT_HALF_OPEN1:
        case SymId::PICT_HALF_OPEN2:
            return String(u"half-muted smufl=\"%1\"").arg(String::from_ascii(SymNames::name_for_sym_id(sid).ascii()));
        case SymId::GUITAR_OPEN_PEDAL:
        case SymId::PICT_OPEN: return String(u"open smufl=\"%1\"").arg(String::from_ascii(SymNames::name_for_sym_id(sid).ascii()));
        default: return String(); // nothing
    }
}

//---------------------------------------------------------
//   writeChordLines
//---------------------------------------------------------

static void write_chord_lines(const Chord *const chord, XmlWriter &xml, Notations &notations) {
    for (EngravingItem *e : chord->el()) {
        if (e->is_chord_line()) {
            const ChordLine *cl = to_chord_line(e);
            String subtype;
            switch (cl->chord_line_type()) {
                case ChordLineType::FALL: subtype = u"falloff"; break;
                case ChordLineType::DOIT: subtype = u"doit"; break;
                case ChordLineType::PLOP: subtype = u"plop"; break;
                case ChordLineType::SCOOP: subtype = u"scoop"; break;
                default: LOGD("unknown ChordLine subtype %d", int(cl->chord_line_type()));
            }
            if (!subtype.empty()) {
                subtype += color2xml(cl);
                if (cl->is_straight()) {
                    subtype += u" line-shape=\"straight\"";
                }
                if (cl->is_wavy()) {
                    subtype += u" line-type=\"wavy\"";
                }
                notations.tag(xml, cl, "articulations");
                xml.tag_raw(subtype);
            }
        }
    }
}

//---------------------------------------------------------
//   writeBreathMark
//---------------------------------------------------------

static void write_breath_mark(const Breath *const breath, XmlWriter &xml, Notations &notations, bool mu3_compat) {
    if (breath) {
        String tag_name;
        String type;

        notations.tag(xml, breath, "articulations");
        if (breath->is_caesura()) {
            tag_name = u"caesura";
            switch (breath->sym_id()) {
                case SymId::CAESURA_CURVED: type = u"curved"; break;
                case SymId::CAESURA_SHORT: type = u"short"; break;
                case SymId::CAESURA_THICK: type = u"thick"; break;
                case SymId::CAESURA_SINGLE_STROKE:
                case SymId::CHANT_CAESURA: type = u"single"; break;
                case SymId::CAESURA:
                default:; //type = u"normal"; // is correct too, but see below
            }
        } else {
            tag_name = u"breath-mark";
            switch (breath->sym_id()) {
                case SymId::BREATH_MARK_TICK: type = u"tick"; break;
                case SymId::BREATH_MARK_UPBOW: type = u"upbow"; break;
                case SymId::BREATH_MARK_SALZEDO: type = u"salzedo"; break;
                case SymId::BREATH_MARK_COMMA:
                default: type = u"comma";
            }
        }
        tag_name += color2xml(breath);
        tag_name += ExportMusicXml::positioning_attributes(breath);
        if (breath->placement() == PlacementV::BELOW) {
            tag_name += u" placement=\"below\"";
        } else if (mu3_compat) {
            // MuseScore versions prior to 4.3 otherwise default to below on import
            tag_name += u" placement=\"above\"";
        }

        if (breath->is_caesura() && (type.is_empty() || mu3_compat)) {
            // for backwards compatibility, as 3.6.2 and earlier can't import those special caesuras,
            // but reports corruption on all subsequent measures and imports them entirely empty.
            xml.tag_raw(tag_name);
        } else {
            xml.tag_raw(tag_name, type);
        }
    }
}

//---------------------------------------------------------
//   chordAttributes
//---------------------------------------------------------

void ExportMusicXml::chord_attributes(Chord *chord, Notations &notations, TrillHash &trill_start, TrillHash &trill_stop) {
    if (!chord->is_grace()) {
        std::vector<EngravingItem *> fl;
        for (EngravingItem *e : chord->segment()->annotations()) {
            if (e->track() == chord->track() && e->is_fermata()) {
                fl.push_back(e);
            }
        }
        fermatas(fl, xml_, notations);
    }

    const std::vector<Articulation *> na = chord->articulations();
    // first the attributes whose elements are children of <articulations>
    for (const Articulation *a : na) {
        SymId sid = a->sym_id();
        std::vector<String> mxml_artics = sym_id_to_artic(sid);

        for (String mxml_artic : mxml_artics) {
            if (mxml_artic == u"strong-accent") {
                if (a->up()) {
                    mxml_artic += u" type=\"up\"";
                } else {
                    mxml_artic += u" type=\"down\"";
                }
            } else if (a->anchor() != ArticulationAnchor::AUTO) {
                if (a->anchor() == ArticulationAnchor::TOP) {
                    mxml_artic += u" placement=\"above\"";
                } else {
                    mxml_artic += u" placement=\"below\"";
                }
            }
            mxml_artic += color2xml(a);
            mxml_artic += ExportMusicXml::positioning_attributes(a);

            notations.tag(xml_, a, "articulations");
            xml_.tag_raw(mxml_artic);
        }
    }

    write_breath_mark(chord->has_breath_mark(), xml_, notations, configuration()->export_mu3_compat());
    write_chord_lines(chord, xml_, notations);

    notations.close_container(xml_); // end articulations tag

    // then the attributes whose elements are children of <ornaments>
    for (const Articulation *art : na) {
        if (!art->is_ornament()) {
            continue;
        }
        const Ornament *ornam = to_ornament(art);
        const SymId sid = ornam->sym_id();
        String mxml_ornam = sym_id_to_ornam(sid);

        String placement;
        if (!ornam->is_styled(Pid::ARTICULATION_ANCHOR) && ornam->anchor() != ArticulationAnchor::AUTO) {
            placement = (ornam->anchor() == ArticulationAnchor::BOTTOM ? u"below" : u"above");
        }
        if (!placement.empty()) {
            mxml_ornam += String(u" placement=\"%1\"").arg(placement);
        }
        mxml_ornam += color2xml(ornam);

        notations.tag(xml_, ornam, "ornaments");
        xml_.tag_raw(mxml_ornam);
        for (const Accidental *accidental : ornam->accidentals_above_and_below()) {
            write_accidental(xml_, u"accidental-mark", accidental);
        }
    }
    tremolo_single_start_stop(chord, notations, xml_);
    wavy_line_start_stop(chord, notations, trill_start, trill_stop);
    notations.close_container(xml_); // end ornaments tag

    // and finally the attributes whose elements are children of <technical>
    for (const Articulation *a : na) {
        SymId sid = a->sym_id();
        String placement;
        String direction;

        if (!a->is_styled(Pid::ARTICULATION_ANCHOR) && a->anchor() != ArticulationAnchor::AUTO) {
            placement = (a->anchor() == ArticulationAnchor::BOTTOM) ? u"below" : u"above";
        } else if (!a->is_styled(Pid::DIRECTION) && a->direction() != DirectionV::AUTO) {
            direction = (a->direction() == DirectionV::DOWN) ? u"down" : u"up";
        }
        /* For future use if/when implemented font details for articulation
        if (!a->isStyled(Pid::FONT_FACE))
              attr += String(" font-family=\"%1\"").arg(a->getProperty(Pid::FONT_FACE).toString());
        if (!a->isStyled(Pid::FONT_SIZE))
              attr += String(" font-size=\"%1\"").arg(a->getProperty(Pid::FONT_SIZE).toReal());
        if (!a->isStyled(Pid::FONT_STYLE))
              attr += fontStyleToXML(static_cast<FontStyle>(a->getProperty(Pid::FONT_STYLE).toInt()), false);
        */

        String mxml_techn = sym_id_to_techn(sid);
        if (!mxml_techn.empty()) {
            notations.tag(xml_, a, "technical");
            mxml_techn += color2xml(a);
            mxml_techn += ExportMusicXml::positioning_attributes(a);
            if (!placement.empty()) {
                mxml_techn += String(u" placement=\"%1\"").arg(placement);
            }
            if (sid == SymId::STRINGS_HARMONIC) {
                xml_.start_element_raw(mxml_techn);
                xml_.tag("natural");
                xml_.end_element();
            } else if (sid == SymId::HANDBELLS_MALLET_BELL_SUSPENDED) {
                // special case for mallet bell suspended
                xml_.tag_raw(mxml_techn);
            } else if (String::from_ascii(SymNames::name_for_sym_id(sid).ascii()).starts_with(u"handbells")) {
                String handbell = u"handbell";
                handbell += color2xml(a);
                handbell += ExportMusicXml::positioning_attributes(a);
                if (!placement.empty()) {
                    handbell += String(u" placement=\"%1\"").arg(placement);
                }
                xml_.tag_raw(handbell, sym_id_to_techn(sid));
            } else if (mxml_techn.starts_with(u"harmon")) {
                xml_.start_element_raw(mxml_techn);
                XmlWriter::Attributes location = {};
                String harmon_closed_value;
                switch (sid) {
                    case SymId::BRASS_HARMON_MUTE_CLOSED: harmon_closed_value = u"yes"; break;
                    case SymId::BRASS_HARMON_MUTE_STEM_OPEN: harmon_closed_value = u"no"; break;
                    default:
                        harmon_closed_value = u"half";
                        location = {{"location", (sid == SymId::BRASS_HARMON_MUTE_STEM_HALF_LEFT) ? "left" : "right"}};
                        break;
                }
                xml_.tag("harmon-closed", location, harmon_closed_value);
                xml_.end_element();
            } else if (mxml_techn.starts_with(u"hole")) {
                xml_.start_element_raw(mxml_techn);
                XmlWriter::Attributes location = {};
                String hole_closed_value;
                switch (sid) {
                    case SymId::WIND_CLOSED_HOLE: hole_closed_value = u"yes"; break;
                    case SymId::WIND_OPEN_HOLE: hole_closed_value = u"no"; break;
                    default:
                        hole_closed_value = u"half";
                        location = {{"location", (sid == SymId::WIND_HALF_CLOSED_HOLE1) ? "right" : "bottom"}};
                        break;
                }
                xml_.tag("hole-closed", location, hole_closed_value);
                xml_.end_element();
            } else {
                xml_.tag_raw(mxml_techn);
            }
        } else if (a->is_tapping()) {
            const Tapping *tap = to_tapping(a);
            notations.tag(xml_, a, "technical");
            mxml_techn = u"tap";
            if (tap->hand() != TappingHand::INVALID) {
                mxml_techn += String(u" hand=\"%1\"").arg(String::from_ascii(TConv::to_xml(tap->hand()).ascii()));
            }
            mxml_techn += color2xml(a);
            mxml_techn += ExportMusicXml::positioning_attributes(a);
            if (!placement.empty()) {
                mxml_techn += String(u" placement=\"%1\"").arg(placement);
            }
            xml_.tag_raw(mxml_techn);
        }
    }

    // write all remaining articulations as other-articulation
    for (const Articulation *a : na) {
        const SymId sid = a->sym_id();
        const AsciiStringView artic_text = TConv::to_xml(a->text_type());
        if (sym_id_to_artic(sid).empty() && sym_id_to_techn(sid).empty() && !a->is_ornament() && !a->is_tapping() &&
            !is_laissez_vibrer(sid)) {
            String other_artic = u"other-articulation";
            other_artic += color2xml(a);
            other_artic += ExportMusicXml::positioning_attributes(a);
            if (a->anchor() != ArticulationAnchor::AUTO) {
                if (a->anchor() == ArticulationAnchor::TOP) {
                    other_artic += u" placement=\"above\"";
                } else {
                    other_artic += u" placement=\"below\"";
                }
            }
            AsciiStringView artic_glyph = SymNames::name_for_sym_id(sid);
            if (!artic_glyph.empty()) {
                other_artic += String(u" smufl=\"%1\"").arg(String::from_ascii(artic_glyph.ascii()));
            }
            if (!artic_glyph.empty() || !artic_text.empty()) {
                notations.tag(xml_, a, "articulations");
                xml_.tag_raw(other_artic, artic_text);
                notations.etag(xml_);
            }
        }
    }
}

//---------------------------------------------------------
//   arpeggiate
//---------------------------------------------------------

// <notations>
//   <arpeggiate direction="up"/>
//   </notations>

static void arpeggiate(Arpeggio *arp, bool front, bool back, XmlWriter &xml, Notations &notations, ArpeggioMap &arps,
                       bool span_arp = false) {
    bool found = false;
    int arp_no = 1;

    // Number arpeggios spanning multiple voices correctly
    const std::vector<MusicXmlArpeggioDesc> found_arps = muse::values(arps, arp->tick().ticks());
    for (const MusicXmlArpeggioDesc arp_desc : found_arps) {
        if (arp_desc.arp == arp) {
            found = true;
            arp_no = arp_desc.no;
        } else {
            arp_no = std::max(arp_no, arp_desc.no) + 1;
        }
    }

    if (!found && span_arp) {
        LOGD("span arpeggio without main arpeggio found at tick %d", arp->tick().ticks());
        return;
    }

    String tag_name;
    switch (arp->arpeggio_type()) {
        case ArpeggioType::NORMAL:
            notations.tag(xml, arp);
            tag_name = u"arpeggiate number=\"" + String::number(arp_no) + u"\"";
            break;
        case ArpeggioType::UP:          // fall through
        case ArpeggioType::UP_STRAIGHT: // not supported by MusicXML, export as normal arpeggio
            notations.tag(xml, arp);
            tag_name = u"arpeggiate direction=\"up\" number=\"" + String::number(arp_no) + u"\"";
            break;
        case ArpeggioType::DOWN:          // fall through
        case ArpeggioType::DOWN_STRAIGHT: // not supported by MusicXML, export as normal arpeggio
            notations.tag(xml, arp);
            tag_name = u"arpeggiate direction=\"down\" number=\"" + String::number(arp_no) + u"\"";
            break;
        case ArpeggioType::BRACKET:
            if (front) {
                notations.tag(xml, arp);
                tag_name = u"non-arpeggiate type=\"bottom\" number=\"" + String::number(arp_no) + u"\"";
            }
            if (back) {
                notations.tag(xml, arp);
                tag_name = u"non-arpeggiate type=\"top\" number=\"" + String::number(arp_no) + u"\"";
            }
            break;
        default: LOGD("unknown arpeggio subtype %d", int(arp->arpeggio_type())); break;
    }

    if (!tag_name.empty()) {
        tag_name += color2xml(arp);
        tag_name += ExportMusicXml::positioning_attributes(arp);
        xml.tag_raw(tag_name);
        if (!found) {
            MusicXmlArpeggioDesc arp_desc(arp, arp_no);
            arps.insert(std::pair<int, MusicXmlArpeggioDesc>(arp->tick().ticks(), arp_desc));
        }
    }
}

//---------------------------------------------------------
//   determineTupletNormalTicks
//---------------------------------------------------------

/**
 Determine the ticks in the normal type for the tuplet \a chord.
 This is non-zero only if chord if part of a tuplet containing
 different length duration elements.
 TODO determine how to handle baselen with dots and verify correct behaviour.
 TODO verify if baseLen should always be correctly set
      (it seems after MusicXMLimport this is not the case)
 */

static int determine_tuplet_normal_ticks(Tuplet const *const t) {
    if (!t) {
        return 0;
    }
    /*
    LOGD("determineTupletNormalTicks t %p baselen %s", t, muPrintable(t->baseLen().ticks().print()));
    for (int i = 0; i < t->elements().size(); ++i)
          LOGD("determineTupletNormalTicks t %p i %d ticks %s", t, i, muPrintable(t->elements().at(i)->ticks().print()));
          */
    for (unsigned int i = 1; i < t->elements().size(); ++i) {
        if (t->elements().at(0)->ticks() != t->elements().at(i)->ticks()) {
            return t->base_len().ticks().ticks();
        }
    }
    if (t->elements().size() != (unsigned) (t->ratio().numerator())) {
        return t->base_len().ticks().ticks();
    }
    return 0;
}

//---------------------------------------------------------
//   beamFanAttribute
//---------------------------------------------------------

static String beam_fan_attribute(const Beam *const b) {
    const double epsilon = 0.1;

    String fan;
    if ((b->grow_right() - b->grow_left() > epsilon)) {
        fan = u"accel";
    }

    if ((b->grow_left() - b->grow_right() > epsilon)) {
        fan = u"rit";
    }

    if (!fan.empty()) {
        return String(u" fan=\"%1\"").arg(fan);
    }

    return String();
}

//---------------------------------------------------------
//   writeBeam
//---------------------------------------------------------

//  beaming
//    <beam number="1">start</beam>
//    <beam number="1">end</beam>
//    <beam number="1">continue</beam>
//    <beam number="1">backward hook</beam>
//    <beam number="1">forward hook</beam>

static void write_beam(XmlWriter &xml, ChordRest *const cr, Beam *const b) {
    const auto &elements = b->elements();
    const size_t idx = muse::index_of(elements, cr);
    if (idx == muse::nidx) {
        LOGD("writeBeam: cannot find ChordRest");
        return;
    }
    int blp = -1;                  // beam level previous chord or rest
    int blc = -1;                  // beam level current chord or rest
    int bln = -1;                  // beam level next chord or rest
    BeamMode bmc = BeamMode::AUTO; // beam mode current chord or rest
    BeamMode bmn = BeamMode::AUTO; // beam mode next chord or rest
    // find beam level previous chord or rest
    for (size_t i = idx - 1; blp == -1 && i != muse::nidx; --i) {
        const ChordRest *crst = elements[i];
        blp = crst->beams();
    }
    // find beam level current chord or rest
    blc = cr->beams();
    bmc = cr->beam_mode();
    // find beam level next chord or rest
    for (size_t i = idx + 1; bln == -1 && i < elements.size(); ++i) {
        const ChordRest *crst = elements[i];
        bln = crst->beams();
        bmn = crst->beam_mode();
    }
    // find beam type and write
    for (int i = 1; i <= blc; ++i) {
        String text;
        // TODO: correctly handle Beam::Mode::AUTO
        // when equivalent to BEGIN32 or BEGIN64
        if ((blp < i && bln >= i) || (bmc == BeamMode::BEGIN16 && i > 1) || (bmc == BeamMode::BEGIN32 && i > 2)) {
            text = u"begin";
        } else if (blp < i && bln < i) {
            if (bln > 0) {
                text = u"forward hook";
            } else if (blp > 0) {
                text = u"backward hook";
            }
        } else if ((blp >= i && bln < i) || (bmn == BeamMode::BEGIN16 && i > 1) || (bmn == BeamMode::BEGIN32 && i > 2)) {
            text = u"end";
        } else if (blp >= i && bln >= i) {
            text = u"continue";
        }
        if (!text.empty()) {
            String tag = u"beam";
            tag += String(u" number=\"%1\"").arg(i);
            if (text == u"begin") {
                tag += beam_fan_attribute(b);
                tag += color2xml(b);
            }
            xml.tag_raw(tag, text);
        }
    }
}

//---------------------------------------------------------
//   instrId
//---------------------------------------------------------

static String instr_id(size_t part_nr, int instr_nr) { return String(u"id=\"P%1-I%2\"").arg(part_nr).arg(instr_nr); }

//---------------------------------------------------------
//   isNoteheadParenthesis
//---------------------------------------------------------

static bool is_notehead_parenthesis(const Symbol *symbol) {
    const SymId sym = symbol->sym();
    return sym == SymId::NOTEHEAD_PARENTHESIS_LEFT || sym == SymId::NOTEHEAD_PARENTHESIS_RIGHT;
}

//---------------------------------------------------------
//   writeNotehead
//---------------------------------------------------------

static void write_notehead(XmlWriter &xml, const Note *const note) {
    String notehead_tagname = u"notehead";
    std::string_view notehead_value;
    if (!color2xml(note).empty()) {
        notehead_tagname += color2xml(note);
        notehead_value = "normal";
    }
    if (note->get_property(Pid::HAS_PARENTHESES).value<ParenthesesMode>() != ParenthesesMode::NONE) {
        notehead_tagname += u" parentheses=\"yes\"";
        notehead_value = "normal";
    }
    if (note->head_type() == NoteHeadType::HEAD_QUARTER) {
        notehead_tagname += u" filled=\"yes\"";
    } else if ((note->head_type() == NoteHeadType::HEAD_HALF) || (note->head_type() == NoteHeadType::HEAD_WHOLE)) {
        notehead_tagname += u" filled=\"no\"";
    }
    if (!note->visible() && (!note->staff_type() || !note->staff_type()->is_tab_staff())) {
        // The notehead is invisible but other parts of the note might
        // still be visible so don't export <note print-object="no">.
        notehead_value = "none";
    } else if (note->head_scheme() == NoteHeadScheme::HEAD_SHAPE_NOTE_4) {
        const int degree = tpc2degree(note->tpc(), note->staff()->key(note->tick()));
        switch (degree) {
            case 0:
            case 3: note->chord()->up() ? notehead_value = "fa up" : notehead_value = "fa"; break;
            case 1:
            case 4: notehead_value = "so"; break;
            case 2:
            case 5: notehead_value = "la"; break;
            case 6: notehead_value = "mi"; break;
        }
    } else if (note->head_scheme() == NoteHeadScheme::HEAD_SHAPE_NOTE_7_AIKIN ||
               note->head_scheme() == NoteHeadScheme::HEAD_SHAPE_NOTE_7_FUNK ||
               note->head_scheme() == NoteHeadScheme::HEAD_SHAPE_NOTE_7_WALKER) {
        const int degree = tpc2degree(note->tpc(), note->staff()->key(note->tick()));
        switch (degree) {
            case 0: notehead_value = "do"; break;
            case 1: notehead_value = "re"; break;
            case 2: notehead_value = "mi"; break;
            case 3: note->chord()->up() ? notehead_value = "fa up" : notehead_value = "fa"; break;
            case 4: notehead_value = "so"; break;
            case 5: notehead_value = "la"; break;
            case 6: notehead_value = "ti"; break;
        }
    } else if (note->head_group() == NoteHeadGroup::HEAD_SLASH) {
        notehead_value = "slash";
    } else if (note->head_group() == NoteHeadGroup::HEAD_TRIANGLE_UP) {
        notehead_value = "triangle";
    } else if (note->head_group() == NoteHeadGroup::HEAD_DIAMOND) {
        notehead_value = "diamond";
    } else if (note->head_group() == NoteHeadGroup::HEAD_PLUS) {
        notehead_value = "cross";
    } else if (note->head_group() == NoteHeadGroup::HEAD_CROSS) {
        notehead_value = "x";
    } else if (note->head_group() == NoteHeadGroup::HEAD_CIRCLED) {
        notehead_value = "circled";
    } else if (note->head_group() == NoteHeadGroup::HEAD_XCIRCLE) {
        notehead_value = "circle-x";
    } else if (note->head_group() == NoteHeadGroup::HEAD_TRIANGLE_DOWN) {
        notehead_value = "inverted triangle";
    } else if (note->head_group() == NoteHeadGroup::HEAD_SLASHED1) {
        notehead_value = "slashed";
    } else if (note->head_group() == NoteHeadGroup::HEAD_SLASHED2) {
        notehead_value = "back slashed";
    } else if (note->head_group() == NoteHeadGroup::HEAD_DO) {
        notehead_value = "do";
    } else if (note->head_group() == NoteHeadGroup::HEAD_RE) {
        notehead_value = "re";
    } else if (note->head_group() == NoteHeadGroup::HEAD_MI) {
        notehead_value = "mi";
    } else if (note->head_group() == NoteHeadGroup::HEAD_FA && !note->chord()->up()) {
        notehead_value = "fa";
    } else if (note->head_group() == NoteHeadGroup::HEAD_FA && note->chord()->up()) {
        notehead_value = "fa up";
    } else if (note->head_group() == NoteHeadGroup::HEAD_LA) {
        notehead_value = "la";
    } else if (note->head_group() == NoteHeadGroup::HEAD_TI) {
        notehead_value = "ti";
    } else if (note->head_group() == NoteHeadGroup::HEAD_SOL) {
        notehead_value = "so";
    } else if (note->head_group() != NoteHeadGroup::HEAD_NORMAL) {
        AsciiStringView notehead_name = SymNames::name_for_sym_id(note->note_head());
        notehead_tagname += String(u" smufl=\"%1\"").arg(String::from_ascii(notehead_name.ascii()));
        notehead_value = "other";
    } else if (note->head_type() != NoteHeadType::HEAD_AUTO) {
        notehead_value = "normal";
    }
    if (!notehead_value.empty()) {
        xml.tag_raw(notehead_tagname, notehead_value);
    }

    if (note->head_scheme() == NoteHeadScheme::HEAD_PITCHNAME || note->head_scheme() == NoteHeadScheme::HEAD_PITCHNAME_GERMAN ||
        note->head_scheme() == NoteHeadScheme::HEAD_SOLFEGE || note->head_scheme() == NoteHeadScheme::HEAD_SOLFEGE_FIXED) {
        static const std::regex nameparts("^note([A-Z][a-z]*)(Sharp|Flat)?");
        AsciiStringView notehead_name = SymNames::name_for_sym_id(note->note_head());
        StringList matches = String::from_ascii(notehead_name.ascii()).search(nameparts, {1, 2}, SplitBehavior::SKIP_EMPTY_PARTS);
        if (!matches.empty()) {
            xml.start_element("notehead-text");
            xml.tag("display-text", matches.at(0));
            if (matches.size() > 1) {
                xml.tag("accidental-text", matches.at(1).to_lower());
            }
            xml.end_element();
        }
    }
}

//---------------------------------------------------------
//   writeGuitarBend
//---------------------------------------------------------

static void write_guitar_bend(XmlWriter &xml, Notations &notations, const Note *const note) {
    if (note->bend_back()) {
        const GuitarBend *bend = note->bend_back();
        if (bend->bend_type() == GuitarBendType::PRE_BEND || bend->bend_type() == GuitarBendType::GRACE_NOTE_BEND) {
            XmlWriter::Attributes bend_attrs;
            notations.tag(xml, bend, "technical");
            bend_attrs.push_back({"first-beat", bend->start_time_factor() * 100});
            bend_attrs.push_back({"last-beat", bend->end_time_factor() * 100});
            add_color_attr(bend, bend_attrs);
            xml.start_element("bend", bend_attrs);
            xml.tag("bend-alter", String::number(-0.5 * bend->bend_amount_in_quarter_tones(), 2));
            xml.tag("pre-bend");
            xml.end_element();
        }
    }
    if (note->bend_for()) {
        const GuitarBend *bend = note->bend_for();
        if (bend->bend_type() == GuitarBendType::BEND || bend->bend_type() == GuitarBendType::SLIGHT_BEND) {
            notations.tag(xml, bend, "technical");
            XmlWriter::Attributes bend_attrs;
            bend_attrs.push_back({"first-beat", bend->start_time_factor() * 100});
            bend_attrs.push_back({"last-beat", bend->end_time_factor() * 100});
            add_color_attr(bend, bend_attrs);
            xml.start_element("bend", bend_attrs);
            xml.tag("bend-alter", String::number(0.5 * bend->bend_amount_in_quarter_tones(), 2));
            xml.end_element();
        }
    }
    for (EngravingItem *e : note->chord()->segment()->annotations()) {
        if (e->is_tremolo_bar() && e->track() == note->track()) {
            const TremoloBar *bend = to_tremolo_bar(e);
            notations.tag(xml, note, "technical");
            XmlWriter::Attributes bend_attrs;
            int max_pitch = 0;
            for (const PitchValue &v : bend->points()) {
                if (!v.pitch) {
                    continue;
                }
                max_pitch = v.pitch < 0 ? std::min(max_pitch, v.pitch) : std::max(max_pitch, v.pitch);
            }
            add_color_attr(bend, bend_attrs);
            xml.start_element("bend", bend_attrs);
            xml.tag("bend-alter", String::number(max_pitch / PitchValue::pitch_for_semitone, 2));
            xml.tag("with-bar", TConv::to_xml(static_cast<TremoloBarType>(bend->get_property(Pid::TREMOLOBAR_TYPE).to_int())));
            xml.end_element();
        }
    }
}

//---------------------------------------------------------
//   writeFingering
//---------------------------------------------------------

static void write_fingering(XmlWriter &xml, Notations &notations, const Note *const note) {
    for (const EngravingItem *e : note->el()) {
        if (e->is_fingering()) {
            const TextBase *f = to_text_base(e);
            notations.tag(xml, e, "technical");
            String t = MScoreTextToMusicXml::to_plain_text(f->xml_text());
            String attr;
            if (!f->is_styled(Pid::PLACEMENT)) {
                attr += placement2xml(e);
            }
            if (!f->is_styled(Pid::FONT_FACE)) {
                attr += String(u" font-family=\"%1\"").arg(f->get_property(Pid::FONT_FACE).value<String>());
            }
            if (!f->is_styled(Pid::FONT_SIZE)) {
                attr += String(u" font-size=\"%1\"").arg(f->get_property(Pid::FONT_SIZE).to_real());
            }
            if (!f->is_styled(Pid::FONT_STYLE)) {
                attr += font_style_to_xml(static_cast<FontStyle>(f->get_property(Pid::FONT_STYLE).to_int()), false);
            }
            attr += color2xml(f);
            attr += ExportMusicXml::positioning_attributes(f);

            if (f->text_style_type() == TextStyleType::RH_GUITAR_FINGERING) {
                xml.tag_raw(u"pluck" + attr, t);
            } else if (f->text_style_type() == TextStyleType::LH_GUITAR_FINGERING) {
                xml.tag_raw(u"fingering" + attr, t);
            } else if (f->text_style_type() == TextStyleType::FINGERING) {
                // for generic fingering, try to detect plucking
                // (backwards compatibility with MuseScore 1.x)
                // p, i, m, a, c represent the plucking finger
                if (t == "p" || t == "i" || t == "m" || t == "a" || t == "c") {
                    xml.tag_raw(u"pluck" + attr, t);
                } else {
                    xml.tag_raw(u"fingering" + attr, t);
                }
            } else if (f->text_style_type() == TextStyleType::STRING_NUMBER) {
                bool ok;
                int i = t.to_int(&ok);
                if (ok) {
                    if (i == 0) {
                        xml.tag_raw(u"open-string" + attr);
                    } else if (i > 0) {
                        xml.tag_raw(u"string" + attr, t);
                    }
                }
                if (!ok || i < 0) {
                    LOGD("invalid string number '%s'", muPrintable(t));
                }
            } else {
                LOGD("unknown fingering style");
            }
        } else {
            // TODO
        }
    }
}

//---------------------------------------------------------
//   writeNotationSymbols
//---------------------------------------------------------

static void write_notation_symbols(XmlWriter &xml, Notations &notations, const ElementList &elist, bool exclude_parentheses) {
    for (const EngravingItem *e : elist) {
        if (!e->is_symbol()) {
            continue;
        }

        const Symbol *symbol = to_symbol(e);

        if (exclude_parentheses && is_notehead_parenthesis(symbol)) {
            // Already handled when writing notehead properties.
            continue;
        }

        notations.tag(xml, symbol);
        xml.tag("other-notation", {{"type", "single"}, {"smufl", symbol->sym_name()}});
    }
}

//---------------------------------------------------------
//   tremoloCorrection
//---------------------------------------------------------

// duration correction for two note tremolo
static int tremolo_correction(const Note *const note) {
    int trem_corr = 1;
    if (is_two_note_tremolo(note->chord())) {
        trem_corr = 2;
    }
    return trem_corr;
}

//---------------------------------------------------------
//   isSmallNote
//---------------------------------------------------------

static bool is_small_note(const Note *const note) { return note->is_small() || note->chord()->is_small(); }

//---------------------------------------------------------
//   isCueNote
//---------------------------------------------------------

static bool is_cue_note(const Note *const note) { return is_small_note(note) && !note->play(); }

//---------------------------------------------------------
//   timeModification
//---------------------------------------------------------

static Fraction time_modification(const Tuplet *const tuplet, const int tremolo = 1) {
    int act_notes{tremolo};
    int nrm_notes{1};
    const Tuplet *t{tuplet};

    while (t) {
        // cannot use Fraction::operator*() as it contains a reduce(),
        // which would change a 6:4 tuplet into 3:2
        act_notes *= t->ratio().numerator();
        nrm_notes *= t->ratio().denominator();
        t = t->tuplet();
    }

    return {act_notes, nrm_notes};
}

//---------------------------------------------------------
//   writeType
//---------------------------------------------------------

static void write_type(XmlWriter &xml, const Note *const note) {
    int dots = 0;
    const Fraction ratio = time_modification(note->chord()->tuplet());

    const Fraction str_act_fraction = note->chord()->global_ticks();
    const Fraction tt = str_act_fraction * ratio * tremolo_correction(note);
    const String s = tick2xml(tt, &dots);
    if (s.is_empty()) {
        LOGD("no note type found for fraction %d / %d", str_act_fraction.numerator(), str_act_fraction.denominator());
    }

    // small notes are indicated by size=cue, but for grace and cue notes this is implicit
    if (is_small_note(note) && !is_cue_note(note) && !note->chord()->is_grace()) {
        xml.tag("type", {{"size", "cue"}}, s);
    } else if (is_small_note(note) && !is_cue_note(note) && note->chord()->is_grace()) {
        xml.tag("type", {{"size", "grace-cue"}}, s);
    } else {
        xml.tag("type", s);
    }

    if (note->dots().empty()) {
        for (int ni = dots; ni > 0; --ni) {
            xml.tag("dot");
        }
    }
}

//---------------------------------------------------------
//   writeTimeModification
//---------------------------------------------------------

static void write_time_modification(XmlWriter &xml, const Tuplet *const tuplet, const int tremolo = 1) {
    const Fraction ratio = time_modification(tuplet, tremolo);
    if (ratio != Fraction(1, 1)) {
        const int act_notes = ratio.numerator();
        const int nrm_notes = ratio.denominator();

        const int nrm_ticks = determine_tuplet_normal_ticks(tuplet);
        xml.start_element("time-modification");
        xml.tag("actual-notes", act_notes);
        xml.tag("normal-notes", nrm_notes);
        //LOGD("nrmTicks %d", nrmTicks);
        if (nrm_ticks > 0) {
            int nrm_dots{0};
            const String nrm_type = tick2xml(Fraction::from_ticks(nrm_ticks), &nrm_dots);
            if (nrm_type.is_empty()) {
                LOGD("no note type found for ticks %d", nrm_ticks);
            } else {
                xml.tag("normal-type", nrm_type);
                for (int ni = nrm_dots; ni > 0; ni--) {
                    xml.tag("normal-dot");
                }
            }
        }
        xml.end_element();
    }
}

//---------------------------------------------------------
//   writePitch
//---------------------------------------------------------

static void write_pitch(XmlWriter &xml, const Note *const note, const bool use_drumset) {
    // step / alter / octave
    String step;
    int alter = 0;
    int octave = 0;
    if (!use_drumset) {
        pitch2xml(note, step, alter, octave);
    } else {
        unpitch2xml(note, step, octave);
    }
    xml.start_element(use_drumset ? "unpitched" : "pitch");
    xml.tag(use_drumset ? "display-step" : "step", step);
    // Check for microtonal accidentals and overwrite "alter" tag
    double microtonal_alter = note->cent_offset() / 100.0;
    // Explicit note tuning
    double tuning = note->tuning();
    if (!muse::real_is_null(tuning)) {
        microtonal_alter += tuning / 100.0;
    }
    if (alter || microtonal_alter) {
        xml.tag("alter", alter + microtonal_alter);
    }
    xml.tag(use_drumset ? "display-octave" : "octave", octave);
    xml.end_element();
}

//---------------------------------------------------------
//   elementPosition
//---------------------------------------------------------

String ExportMusicXml::element_position(const ExportMusicXml *const exp_mxml, const EngravingItem *const elm) {
    String res;

    if (exp_mxml->configuration()->export_layout()) {
        const double page_height = exp_mxml->get_tenths_from_inches(exp_mxml->score()->style().style_d(Sid::PAGE_HEIGHT));

        const Measure *meas = elm->find_measure();
        IF_ASSERT_FAILED(meas) { return res; }

        double measure_x = exp_mxml->get_tenths_from_dots(meas->page_pos().x());
        double measure_y = page_height - exp_mxml->get_tenths_from_dots(meas->page_pos().y());
        double note_x = exp_mxml->get_tenths_from_dots(elm->page_pos().x());
        double note_y = page_height - exp_mxml->get_tenths_from_dots(elm->page_pos().y());

        res += String(u" default-x=\"%1\"").arg(String::number(note_x - measure_x, 2));
        res += String(u" default-y=\"%1\"").arg(String::number(note_y - measure_y, 2));
    }

    return res;
}

//---------------------------------------------------------
//   chord
//---------------------------------------------------------

/**
 Write \a chord on \a staff with lyriclist \a ll.

 For a single-staff part, \a staff equals zero, suppressing the <staff> element.
 */

void ExportMusicXml::chord(Chord *chord, staff_idx_t staff, const std::vector<Lyrics *> &ll, bool use_drumset) {
    Part *part = chord->score()->staff(chord->track() / ::mu::engraving::voices)->part();
    size_t part_nr = muse::index_of(score_->parts(), part);
    int inst_nr = muse::value(instr_map_, part->instrument(tick_), -1);
    /*
    LOGD("chord() %p parent %p isgrace %d #gracenotes %d graceidx %d",
           chord, chord->parent(), chord->isGrace(), chord->graceNotes().size(), chord->graceIndex());
    LOGD("track %d tick %d part %p nr %d instr %p nr %d",
           chord->track(), chord->tick(), part, partNr, part->instrument(tick), instNr);
    for (EngravingItem* e : chord->el())
          LOGD("chord %p el %p", chord, e);
     */
    std::vector<Note *> nl = chord->notes();
    bool grace = chord->is_grace();
#ifdef DEBUG_TICK
    LOGD() << "oldtick " << fractionToStdString(tick()) << " grace " << grace;
#endif
    if (!grace) {
        tick_ += chord->actual_ticks();
    }
#ifdef DEBUG_TICK
    LOGD() << "newtick " << fractionToStdString(tick());
#endif

    for (Note *note : nl) {
        attr_.do_attr(xml_, false);
        String note_tag = u"note";

        note_tag += element_position(this, note);

        int velo = note->user_velocity();
        if (velo != 0) {
            note_tag += String(u" dynamics=\"%1\"").arg(String::number(velo * 100.0 / 90.0, 2));
        }

        xml_.start_element_raw(note_tag);

        if (grace) {
            if (note->chord()->show_stem_slash()) {
                xml_.tag("grace", {{"slash", "yes"}});
            } else {
                xml_.tag("grace");
            }
        }
        if (is_cue_note(note)) {
            xml_.tag("cue");
        }
        if (note != nl.front()) {
            xml_.tag("chord");
        }

        write_pitch(xml_, note, use_drumset);

        // duration
        if (!grace) {
            xml_.tag("duration", calculate_duration_in_divisions(chord->global_ticks(), div_));
        }

        if (!is_cue_note(note)) {
            if (note->tie_back_non_partial()) {
                xml_.tag("tie", {{"type", "stop"}});
            }
            if (note->tie_for_non_partial()) {
                xml_.tag("tie", {{"type", "start"}});
            }
        }

        // instrument for multi-instrument or unpitched parts
        if (!use_drumset) {
            if (instr_map_.size() > 1 && inst_nr >= 0) {
                xml_.tag_raw(String(u"instrument %1").arg(instr_id(part_nr + 1, inst_nr + 1)));
            }
        } else {
            xml_.tag_raw(String(u"instrument %1").arg(instr_id(part_nr + 1, note->pitch() + 1)));
        }

        // voice
        // for a single-staff part, staff is 0, which needs to be corrected
        // to calculate the correct voice number
        voice_idx_t voice = (static_cast<int>(staff) - 1) * ::mu::engraving::voices + note->chord()->voice() + 1;
        if (staff == 0) {
            voice += ::mu::engraving::voices;
        }

        xml_.tag("voice", static_cast<int>(voice));

        write_type(xml_, note);
        for (const NoteDot *dot : note->dots()) {
            String dot_tag = u"dot";
            if (note->user_dot_position() != engraving::DirectionV::AUTO) {
                if (note->dot_position() == engraving::DirectionV::UP) {
                    dot_tag += u" placement=\"above\"";
                } else {
                    dot_tag += u" placement=\"below\"";
                }
            }
            dot_tag += color2xml(dot);
            dot_tag += element_position(this, dot);
            xml_.tag_raw(dot_tag);
        }
        if (note->staff() && !note->staff()->is_tab_staff(Fraction(0, 1))) {
            write_accidental(xml_, u"accidental", note->accidental());
        }
        write_time_modification(xml_, note->chord()->tuplet(), tremolo_correction(note));

        // no stem for whole notes and beyond
        if (chord->no_stem() || chord->measure()->stemless(chord->staff_idx()) || (chord->stem() && !chord->stem()->visible())) {
            xml_.tag("stem", "none");
        } else if (const Stem *stem = note->chord()->stem()) {
            String stem_tag = u"stem";
            stem_tag += color2xml(stem);
            xml_.tag_raw(stem_tag, note->chord()->up() ? "up" : "down");
        }

        write_notehead(xml_, note);

        // LVIFIX: check move() handling
        if (staff) {
            xml_.tag("staff", static_cast<int>(staff + note->chord()->staff_move()));
        }

        if (note == nl.front() && chord->beam()) {
            write_beam(xml_, chord, chord->beam());
        }

        Notations notations;

        const Tie *tie_back = note->tie_back();
        if (tie_back) {
            notations.tag(xml_, tie_back);
            xml_.tag("tied", {{"type", "stop"}});
        }

        const LaissezVib *laissez_vib = note->laissez_vib();
        if (laissez_vib) {
            notations.tag(xml_, laissez_vib);
            String rest = slur_tie_line_style(laissez_vib);
            xml_.tag_raw(String(u"tied type=\"let-ring\"%1").arg(rest));
        }

        const Tie *tie_for = note->tie_for();
        if (tie_for && !laissez_vib) {
            notations.tag(xml_, tie_for);
            String rest = slur_tie_line_style(tie_for);
            xml_.tag_raw(String(u"tied type=\"start\"%1").arg(rest));
        }

        if (note == nl.front()) {
            if (!grace) {
                tuplet_start_stop(chord, notations, xml_);
            }

            sh_.do_slurs(chord, notations, xml_);

            chord_attributes(chord, notations, trill_start_, trill_stop_);
        }

        write_fingering(xml_, notations, note);
        write_notation_symbols(xml_, notations, note->el(), true);

        // write tablature string / fret
        if (chord->staff() && chord->staff()->is_tab_staff(Fraction(0, 1))) {
            if (note->fret() >= 0 && note->string() >= 0) {
                notations.tag(xml_, note, "technical");
                xml_.tag("string", note->string() + 1);
                xml_.tag("fret", note->fret());
            }
        }

        write_guitar_bend(xml_, notations, note);
        notations.close_container(xml_); // end technical tag

        if (chord->arpeggio()) {
            arpeggiate(chord->arpeggio(), note == nl.front(), note == nl.back(), xml_, notations, meas_arpeggios_);
        } else if (chord->span_arpeggio()) {
            arpeggiate(chord->span_arpeggio(), note == nl.front(), note == nl.back(), xml_, notations, meas_arpeggios_, /*spanArp=*/true);
        }
        for (Spanner *spanner : note->spanner_for()) {
            if (spanner->is_glissando()) {
                gh_.do_glissando_start(to_glissando(spanner), notations, xml_);
            }
        }
        for (Spanner *spanner : note->spanner_back()) {
            if (spanner->is_glissando()) {
                gh_.do_glissando_stop(to_glissando(spanner), notations, xml_);
            }
        }
        // write glissando (only for last note)
        /*
        Chord* ch = nextChord(chord);
        if ((note == nl.back()) && ch && ch->glissando()) {
              gh.doGlissandoStart(ch, notations, xml);
              }
        if (chord->glissando()) {
              gh.doGlissandoStop(chord, notations, xml);
              }
        */
        notations.etag(xml_);
        // write lyrics (only for first note)
        if (!grace && (note == nl.front())) {
            lyrics(ll, chord->track());
        }
        xml_.end_element();
    }
}

//---------------------------------------------------------
//   rest
//---------------------------------------------------------

/**
 Write \a rest on \a staff.

 For a single-staff part, \a staff equals zero, suppressing the <staff> element.
 */

void ExportMusicXml::rest(Rest *rest, staff_idx_t staff, const std::vector<Lyrics *> &ll) {
    static char16_t table2[] = u"CDEFGAB";
#ifdef DEBUG_TICK
    LOGD() << "oldtick " << fractionToStdString(tick());
#endif
    attr_.do_attr(xml_, false);

    String note_tag = u"note";
    note_tag += color2xml(rest);
    note_tag += element_position(this, rest);
    if (!rest->visible() || (rest->staff() && rest->staff()->staff_type(rest->tick())->is_tab_staff() &&
                             !rest->staff()->staff_type(rest->tick())->show_rests())) {
        note_tag += u" print-object=\"no\"";
    }
    xml_.start_element_raw(note_tag);

    int y_offs_st = 0;
    int oct = 0;
    int stp = 0;
    ClefType clef = rest->staff()->clef(rest->tick());
    int po = ClefInfo::pitch_offset(clef);

    // Determine y position, but leave at zero in case of tablature staff
    // as no display-step or display-octave should be written for a tablature staff,

    if (clef != ClefType::TAB && clef != ClefType::TAB_SERIF && clef != ClefType::TAB4 && clef != ClefType::TAB4_SERIF) {
        double y_offs_sp = -2 * rest->offset().y() / rest->spatium();         // positive = up, one spatium is two pitches
        y_offs_st = int(y_offs_sp > 0.0 ? y_offs_sp + 0.5 : y_offs_sp - 0.5); // same rounded to int

        po -= 4;         // pitch middle staff line (two lines times two steps lower than top line)
        po += y_offs_st; // rest "pitch"
        po -= 7;         // correct octave
        // at this point:
        // C0 (lowest value allowed in MusicXML) has po == 0   (y offset = 17 (treble clef) 11 (bass clef))
        // B9 (highest value allowed in MusicXML) has po == 69 (y offset = -17.5 (treble clef) -23.5 (bass clef))

        po = std::clamp(po, 0, 69);
        oct = po / 7; // octave (MusicXML spec: must be 0..9, C4 is middle C)
        stp = po % 7; // step (must be 0..6, as display-step must be A..G)
        LOGN() << " pitchOffset (initial po) " << ClefInfo::pitch_offset(clef) << " offset().y() " << rest->offset().y() << " spatium() "
               << rest->spatium() << " yOffsSp " << y_offs_sp << " yOffsSt " << y_offs_st << " po " << po << " oct " << oct << " stp "
               << stp << " (" << char(table2[stp]) << oct << ")";
    }

    String rest_tag = u"rest";
    const TDuration d = rest->duration_type();
    if (d.is_measure()) {
        rest_tag += u" measure=\"yes\"";
    }
    // Either <rest/>
    // or <rest><display-step>F</display-step><display-octave>5</display-octave></rest>
    if (y_offs_st == 0) {
        xml_.tag_raw(rest_tag);
    } else {
        xml_.start_element_raw(rest_tag);
        xml_.tag("display-step", String(Char(table2[stp])));
        xml_.tag("display-octave", oct);
        xml_.end_element();
    }

    Fraction tick_len;
    // regular rest
    tick_len = rest->global_ticks(); // MusicXML requires unstretched duration
    tick_ += rest->actual_ticks();   // prevent <backward> or <forward> by moving to next note's tick
#ifdef DEBUG_TICK
    LOGD() << "tickLen " << fractionToStdString(tick_len) << "newtick " << fractionToStdString(tick());
#endif

    xml_.tag("duration", calculate_duration_in_divisions(tick_len, div_));

    // for a single-staff part, staff is 0, which needs to be corrected
    // to calculate the correct voice number
    voice_idx_t voice = (static_cast<int>(staff) - 1) * ::mu::engraving::voices + rest->voice() + 1;
    if (staff == 0) {
        voice += ::mu::engraving::voices;
    }
    xml_.tag("voice", static_cast<int>(voice));

    // do not output a "type" element for whole measure rest
    if (!d.is_measure()) {
        AsciiStringView s = TConv::to_xml(d.type());
        if (rest->is_small()) {
            xml_.tag("type", {{"size", "cue"}}, s);
        } else {
            xml_.tag("type", s);
        }
        for (NoteDot *dot : rest->dot_list()) {
            String dot_tag = u"dot";
            dot_tag += color2xml(dot);
            dot_tag += element_position(this, dot);
            xml_.tag_raw(dot_tag);
        }
    }

    write_time_modification(xml_, rest->tuplet());

    if (staff) {
        xml_.tag("staff", static_cast<int>(staff));
    }

    if (rest->beam()) {
        write_beam(xml_, rest, rest->beam());
    }

    Notations notations;
    std::vector<EngravingItem *> fl;
    for (EngravingItem *e : rest->segment()->annotations()) {
        if (e->is_fermata() && e->track() == rest->track()) {
            fl.push_back(e);
        }
    }
    fermatas(fl, xml_, notations);

    write_breath_mark(rest->has_breath_mark(), xml_, notations, configuration()->export_mu3_compat());

    wavy_line_start_stop(rest, notations, trill_start_, trill_stop_);

    write_notation_symbols(xml_, notations, rest->el(), false);

    sh_.do_slurs(rest, notations, xml_);

    tuplet_start_stop(rest, notations, xml_);
    notations.etag(xml_);

    lyrics(ll, rest->track());

    xml_.end_element();
}

//---------------------------------------------------------
//   directionTag
//---------------------------------------------------------

static void direction_tag(const IMusicXmlConfiguration &configuration, XmlWriter &xml, Attributes &attr,
                          EngravingItem const *const el = 0) {
    attr.do_attr(xml, false);
    String tag_name = u"direction";
    if (el) {
        tag_name += placement2xml(el);
        if (el->system_flag() && !configuration.export_mu3_compat()) {
            tag_name += u" system=\"only-top\"";
        }
    }
    xml.start_element_raw(tag_name);
}

//---------------------------------------------------------
//   directionETag
//---------------------------------------------------------

static void direction_e_tag(XmlWriter &xml, staff_idx_t staff, const int offset = 0) {
    if (offset) {
        xml.tag("offset", {{"sound", "yes"}}, offset);
    }
    if (staff) {
        xml.tag("staff", static_cast<int>(staff));
    }
    xml.end_element();
}

//---------------------------------------------------------
//   partGroupStart
//---------------------------------------------------------

static void part_group_start(XmlWriter &xml, int number, const BracketItem *const bracket, const bool barline_span, const bool group_time) {
    xml.start_element("part-group", {{"type", "start"}, {"number", number}});
    String br;
    switch (bracket->bracket_type()) {
        case BracketType::NO_BRACKET: br = u"none"; break;
        case BracketType::NORMAL: br = u"bracket"; break;
        case BracketType::BRACE: br = u"brace"; break;
        case BracketType::LINE: br = u"line"; break;
        case BracketType::SQUARE: br = u"square"; break;
        default: LOGD("bracket subtype %d not understood", int(bracket->bracket_type()));
    }
    if (!br.empty()) {
        String tag = u"group-symbol";
        tag += color2xml(bracket);
        xml.tag_raw(tag, br);
    }
    if (barline_span) {
        xml.tag("group-barline", "yes");
    }
    if (group_time) {
        xml.tag("group-time");
    }
    xml.end_element();
}

//---------------------------------------------------------
//   findMetronome
//---------------------------------------------------------
static size_t index_of(const String &input_src, const std::wregex &re, size_t from, std::wsmatch *m) {
    std::u16string u16 = input_src.to_std_u16_string();
    std::wstring src;
    src.resize(u16.size());

    static_assert(sizeof(wchar_t) >= sizeof(char16_t));

    for (size_t i = 0; i < u16.size(); ++i) {
        src[i] = static_cast<wchar_t>(u16.at(i));
    }

    auto begin = std::wsregex_iterator(src.begin(), src.end(), re);
    auto end = std::wsregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::wsmatch match = *it;
        size_t pos = src.find(match.str(), from);
        if (pos != std::u16string::npos) {
            if (m) {
                *m = match;
            }
            return pos;
        }
    }
    return std::u16string::npos;
}

static bool find_metronome(const std::list<TextFragment> &list,
                           std::list<TextFragment> &words_left, // words left of metronome
                           bool &has_paren,                     // parenthesis
                           String &metro_left,                  // left part of metronome
                           String &metro_right,                 // right part of metronome
                           std::list<TextFragment> &words_right // words right of metronome
) {
    String words = MScoreTextToMusicXml::to_plain_text_plus_symbols(list);
    //LOGD("findMetronome('%s')", muPrintable(words));
    has_paren = false;
    metro_left.clear();
    metro_right.clear();

    int len1 = 0;
    TDuration dur;
    int pos1 = TempoText::find_tempo_duration(words, len1, dur);
    if (pos1 == -1) {
        return false;
    }

    int metro_pos = pos1;
    int metro_len = len1;
    metro_left = words.mid(pos1, len1);

    static const std::wregex equation_reg_ex(L"\\s*=\\s*");
    std::wsmatch equation_match;
    size_t pos_eq = index_of(words, equation_reg_ex, pos1 + len1, &equation_match);

    if (pos_eq != muse::nidx && int(pos_eq) == pos1 + len1) {
        int len_eq = equation_match.length();
        String s_after_eq = words.mid(pos_eq + len_eq);
        int len3 = 0;
        TDuration dur3;
        int pos3 = TempoText::find_tempo_duration(s_after_eq, len3, dur3);
        if (pos3 == -1) {
            // did not find note, try to find a number
            static const std::wregex number_reg_ex(L"\\d+([,\\.]{1}\\d+)?");
            std::wsmatch number_match;
            pos3 = static_cast<int>(index_of(s_after_eq, number_reg_ex, 0, &number_match));
            if (pos3 != 0) {
                pos3 = -1;
            } else {
                len3 = number_match.length();
            }
        }
        if (pos3 == 0) {
            metro_right = s_after_eq.mid(0, len3);
            metro_len = len1 + len_eq + len3;
        } else {
            return false;
        }
    }

    if (metro_pos > 0 && words.at(metro_pos - 1) == '(') {
        if (words.size() > size_t(metro_pos + metro_len) && words.at(metro_pos + metro_len) == ')') {
            has_paren = true;
            metro_pos -= 1;
            metro_len += 2;
        }
    }

    // calculate starting position corrected for surrogate pairs
    // (which were ignored by toPlainTextPlusSymbols())
    int corr_pos = metro_pos;
    for (int i = 0; i < metro_pos; ++i) {
        if (words.at(i).is_high_surrogate()) {
            --corr_pos;
        }
    }
    metro_pos = corr_pos;

    std::list<TextFragment> mid; // not used
    MScoreTextToMusicXml::split(list, metro_pos, metro_len, words_left, mid, words_right);
    return true;
}

//---------------------------------------------------------
//   beatUnit
//---------------------------------------------------------

static void beat_unit(XmlWriter &xml, const TDuration dur) {
    int dots = dur.dots();
    xml.tag("beat-unit", TConv::to_xml(dur.type()));
    while (dots > 0) {
        xml.tag("beat-unit-dot");
        --dots;
    }
}

//---------------------------------------------------------
//   wordsMetronome
//---------------------------------------------------------

static void words_metronome(XmlWriter &xml, const MStyle &s, TextBase const *const text, const int offset) {
    const std::list<TextFragment> list = text->fragment_list();
    std::list<TextFragment> words_left;  // words left of metronome
    bool has_paren;                      // parenthesis
    String metro_left;                   // left part of metronome
    String metro_right;                  // right part of metronome
    std::list<TextFragment> words_right; // words right of metronome

    // set the default words format
    const String mtf = s.style_st(Sid::MUSICAL_TEXT_FONT);
    const CharFormat def_fmt = format_for_words(s);

    if (find_metronome(list, words_left, has_paren, metro_left, metro_right, words_right)) {
        if (words_left.size() > 0) {
            xml.start_element("direction-type");
            String attr = ExportMusicXml::positioning_attributes(text);
            MScoreTextToMusicXml mttm(u"words", attr, def_fmt, mtf);
            mttm.write_text_fragments(words_left, xml);
            xml.end_element();
        }

        xml.start_element("direction-type");
        String tag_name = String(u"metronome parentheses=\"%1\"").arg(has_paren ? u"yes" : u"no");
        tag_name += color2xml(text);
        tag_name += ExportMusicXml::positioning_attributes(text);
        if (!text->visible()) {
            tag_name += u" print-object=\"no\"";
        }
        xml.start_element_raw(tag_name);
        int len1 = 0;
        TDuration dur;
        TempoText::find_tempo_duration(metro_left, len1, dur);
        beat_unit(xml, dur);

        if (!metro_right.empty()) {
            if (TempoText::find_tempo_duration(metro_right, len1, dur) != -1) {
                beat_unit(xml, dur);
            } else {
                xml.tag("per-minute", metro_right);
            }
        }

        xml.end_element();
        xml.end_element();

        if (words_right.size() > 0) {
            xml.start_element("direction-type");
            String attr = ExportMusicXml::positioning_attributes(text);
            MScoreTextToMusicXml mttm(u"words", attr, def_fmt, mtf);
            mttm.write_text_fragments(words_right, xml);
            xml.end_element();
        }
    } else {
        xml.start_element("direction-type");
        String attr;
        attr += frame2xml(text);
        attr += color2xml(text);
        attr += ExportMusicXml::positioning_attributes(text);
        MScoreTextToMusicXml mttm(u"words", attr, def_fmt, mtf);
        //LOGD("words('%s')", muPrintable(text->text()));
        mttm.write_text_fragments(text->fragment_list(), xml);
        xml.end_element();
    }

    if (offset) {
        xml.tag("offset", {{"sound", "yes"}}, offset);
    }
}

//---------------------------------------------------------
//   tempoText
//---------------------------------------------------------

void ExportMusicXml::tempo_text(TempoText const *const text, staff_idx_t staff) {
    const int offset = calculate_time_delta_in_divisions(text->tick(), tick(), div_);
    /*
    LOGD("tick %s text->tick %s offset %d xmlText='%s')",
           muPrintable(tick().print()),
           muPrintable(text->tick().print()),
           offset,
           muPrintable(text->xmlText()));
    */
    attr_.do_attr(xml_, false);

    XmlWriter::Attributes tempo_attrs;
    tempo_attrs = {{"placement", TConv::to_xml(text->placement())}};
    if (text->system_flag() && !configuration()->export_mu3_compat()) {
        tempo_attrs.emplace_back(std::make_pair("system", text->is_linked() ? "also-top" : "only-top"));
    }

    xml_.start_element("direction", tempo_attrs);
    words_metronome(xml_, score_->style(), text, offset);

    if (staff) {
        xml_.tag("staff", static_cast<int>(staff));
    }
    tempo_sound(text);
    xml_.end_element();
}

void ExportMusicXml::tempo_sound(TempoText const *const text) {
    // Format tempo with maximum 2 decimal places, because in some MuseScore files tempo is stored
    // imprecisely and this could cause rounding errors (e.g. 92 BPM would be saved as 91.9998).
    BeatsPerMinute bpm = text->tempo().to_bpm();
    if (text->is_a_tempo() || text->is_tempo_primo()) {
        bpm = score_->tempo(text->tick()).to_bpm();
    }
    double bpm_rounded = round(bpm.val * 100) / 100;
    xml_.tag("sound", {{"tempo", bpm_rounded}});
}

void ExportMusicXml::swing_sound(StaffTextBase const *const text, const bool offset) {
    xml_.start_element("sound");
    xml_.start_element("swing");
    if (!text->swing_parameters().swing_unit) {
        xml_.tag("straight");
    } else {
        const int swing_percentage = text->swing_parameters().swing_ratio;
        const int swing_divisor = std::gcd(text->swing_parameters().swing_ratio, 100);
        xml_.tag("first", 100 / swing_divisor);
        xml_.tag("second", swing_percentage / swing_divisor);
        if (text->swing_parameters().swing_unit == Constants::division / 2) {
            xml_.tag("swing-type", TConv::to_xml(DurationType::V_EIGHTH));
        } else {
            xml_.tag("swing-type", TConv::to_xml(DurationType::V_16TH));
        }
    }
    xml_.end_element();
    if (offset) {
        xml_.tag("offset", calculate_time_delta_in_divisions(text->tick(), tick(), div_));
    }
    xml_.end_element();
}

//---------------------------------------------------------
//   playText
//---------------------------------------------------------

void ExportMusicXml::play_text(PlayTechAnnotation const *const annot, staff_idx_t staff) {
    const int offset = calculate_time_delta_in_divisions(annot->tick(), tick(), div_);

    if (annot->plain_text().empty()) {
        // sometimes empty Texts are present, exporting would result
        // in invalid MusicXML (as an empty direction-type would be created)
        return;
    }

    direction_tag(configuration_, xml_, attr_, annot);
    words_metronome(xml_, score_->style(), annot, offset);

    const PlayingTechniqueType type = annot->technique_type();
    if (type == PlayingTechniqueType::PIZZICATO) {
        xml_.tag("sound", {{"pizzicato", "yes"}});
    } else if ((type != PlayingTechniqueType::PIZZICATO) && (curr_play_technique_ == PlayingTechniqueType::PIZZICATO)) {
        xml_.tag("sound", {{"pizzicato", "no"}});
    } else if ((type != PlayingTechniqueType::UNDEFINED) && (type != PlayingTechniqueType::NATURAL)) {
        xml_.start_element("sound");
        xml_.start_element("play");
        if (type == PlayingTechniqueType::MUTE) {
            xml_.tag("mute", "on");
        } else if (type == PlayingTechniqueType::OPEN) {
            xml_.tag("mute", "off");
        } else {
            xml_.tag("other-play", {{"type", TConv::to_xml(type)}}, TConv::user_name(type).translated());
        }
        xml_.end_element();
        xml_.end_element();
    }
    curr_play_technique_ = type;

    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   words
//---------------------------------------------------------

void ExportMusicXml::words(TextBase const *const text, staff_idx_t staff) {
    const int offset = calculate_time_delta_in_divisions(text->tick(), tick(), div_);
    /*
    LOGD("tick %s text->tick %s offset %d userOff.x=%f userOff.y=%f xmlText='%s' plainText='%s'",
           muPrintable(tick().print()),
           muPrintable(text->tick().print()),
           offset,
           text->offset().x(), text->offset().y(),
           muPrintable(text->xmlText()),
           muPrintable(text->plainText()));
    */

    if (text->plain_text().empty()) {
        // sometimes empty Texts are present, exporting would result
        // in invalid MusicXML (as an empty direction-type would be created)
        return;
    }

    direction_tag(configuration_, xml_, attr_, text);
    words_metronome(xml_, score_->style(), text, offset);

    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   getImageInfo
//---------------------------------------------------------

static void get_image_info(const ImageStoreItem *isi, String &source, String &type) {
    source = String::from_std_string(isi->hash_name());
    String suffix = String::from_std_string(isi->type()).to_lower();

    if (suffix == u"svg" || suffix == u"svgz") {
        type = u"image/svg+xml";
    } else if (suffix == u"png") {
        type = u"image/png";
    } else if (suffix == u"jpg" || suffix == u"jpeg") {
        type = u"image/jpeg";
    } else if (suffix == u"gif") {
        type = u"image/gif";
    } else if (suffix == u"bmp") {
        type = u"image/bmp";
    } else if (suffix == u"tif" || suffix == u"tiff") {
        type = u"image/tiff";
    }

    if (type.is_empty()) {
        const ByteArray &ba = isi->buffer();
        if (ba.size() >= 4) {
            if (ba.at(0) == 0x89 && ba.at(1) == 0x50 && ba.at(2) == 0x4E && ba.at(3) == 0x47) {
                type = u"image/png";
                if (suffix.is_empty()) {
                    source += u".png";
                }
            } else if (ba.at(0) == 0xFF && ba.at(1) == 0xD8 && ba.at(2) == 0xFF) {
                type = u"image/jpeg";
                if (suffix.is_empty()) {
                    source += u".jpg";
                }
            } else if (ba.at(0) == 0x47 && ba.at(1) == 0x49 && ba.at(2) == 0x46 && ba.at(3) == 0x38) {
                type = u"image/gif";
                if (suffix.is_empty()) {
                    source += u".gif";
                }
            } else if (ba.at(0) == 0x42 && ba.at(1) == 0x4D) {
                type = u"image/bmp";
                if (suffix.is_empty()) {
                    source += u".bmp";
                }
            } else if ((ba.at(0) == 0x49 && ba.at(1) == 0x49 && ba.at(2) == 0x2A && ba.at(3) == 0x00) ||
                       (ba.at(0) == 0x4D && ba.at(1) == 0x4D && ba.at(2) == 0x00 && ba.at(3) == 0x2A)) {
                type = u"image/tiff";
                if (suffix.is_empty()) {
                    source += u".tif";
                }
            } else if (ba.at(0) == 0x3C && (ba.at(1) == 0x3F || ba.at(1) == 0x73)) {
                type = u"image/svg+xml";
                if (suffix.is_empty()) {
                    source += u".svg";
                }
            }
        }
    }

    if (type.is_empty()) {
        type = u"application/octet-stream";
    }
}

//---------------------------------------------------------
//   image
//---------------------------------------------------------

void ExportMusicXml::image(const Image *const img, staff_idx_t staff) {
    ImageStoreItem *isi = img->store_item();
    if (!isi) {
        return;
    }

    direction_tag(configuration_, xml_, attr_, img);
    xml_.start_element("direction-type");

    String source;
    String type;
    get_image_info(isi, source, type);

    if (is_mxl_) {
        String img_tag = u"image source=\"" + XmlWriter::xml_string(source) + u"\"";
        img_tag += u" type=\"" + XmlWriter::xml_string(type) + u"\"";

        double width = img->image_width();
        double height = img->image_height();
        if (!img->size_is_spatium()) {
            double sp = img->spatium();
            if (sp > 0.0) {
                width /= sp;
                height /= sp;
            }
        }

        img_tag += u" height=\"" + String::number(height * 10.0, 2) + u"\"";
        img_tag += u" width=\"" + String::number(width * 10.0, 2) + u"\"";
        img_tag += positioning_attributes(img);

        xml_.tag_raw(img_tag);
    } else {
        String other_tag = u"other-direction";
        other_tag += positioning_attributes(img);
        xml_.tag_raw(other_tag, XmlWriter::xml_string(source));
    }
    xml_.end_element(); // direction-type

    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   systemText
//---------------------------------------------------------

void ExportMusicXml::system_text(StaffTextBase const *const text, staff_idx_t staff) {
    const int offset = calculate_time_delta_in_divisions(text->tick(), tick(), div_);

    if (text->plain_text().empty()) {
        // sometimes empty Texts are present, exporting would result
        // in invalid MusicXML (as an empty direction-type would be created)
        return;
    }

    direction_tag(configuration_, xml_, attr_, text);
    words_metronome(xml_, score_->style(), text, offset);

    if (text->swing()) {
        swing_sound(text);
    }

    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   positioningAttributesForTboxText
//---------------------------------------------------------

String ExportMusicXml::positioning_attributes_for_tbox_text(const PointF position, float spatium) {
    if (!configuration()->export_layout()) {
        return String();
    }

    PointF relative; // use zero relative position
    return position_to_string(position, relative, spatium);
}

//---------------------------------------------------------
//   tboxTextAsWords
//---------------------------------------------------------

void ExportMusicXml::tbox_text_as_words(TextBase const *const text, const staff_idx_t staff, const PointF relative_position) {
    if (text->plain_text().empty()) {
        // sometimes empty Texts are present, exporting would result
        // in invalid MusicXML (as an empty direction-type would be created)
        return;
    }

    // set the default words format
    const MStyle &style = score_->style();
    const String mtf = style.style_st(Sid::MUSICAL_TEXT_FONT);
    const CharFormat def_fmt = format_for_words(style);

    xml_.start_element("direction", {{"placement", (relative_position.y() < 0) ? "above" : "below"}});
    xml_.start_element("direction-type");
    String attr;
    attr += frame2xml(text);
    attr += ExportMusicXml::positioning_attributes_for_tbox_text(relative_position, text->spatium());
    attr += u" valign=\"top\"";
    MScoreTextToMusicXml mttm(u"words", attr, def_fmt, mtf);
    mttm.write_text_fragments(text->fragment_list(), xml_);
    xml_.end_element();
    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   rehearsal
//---------------------------------------------------------

void ExportMusicXml::rehearsal(RehearsalMark const *const rmk, staff_idx_t staff) {
    if (rmk->plain_text().empty()) {
        // sometimes empty Texts are present, exporting would result
        // in invalid MusicXML (as an empty direction-type would be created)
        return;
    }

    direction_tag(configuration_, xml_, attr_, rmk);
    xml_.start_element("direction-type");
    String attr;
    if (rmk->circle()) {
        attr = u" enclosure=\"circle\"";
    } else if (!rmk->has_frame()) {
        // special default case
        attr = u" enclosure=\"none\"";
    }
    attr += color2xml(rmk);
    attr += positioning_attributes(rmk);
    if (configuration()->export_layout()) {
        switch (rmk->align().horizontal) {
            case AlignH::LEFT:
                // default in MusicXML
                break;
            case AlignH::HCENTER: attr += u" justify=\"center\""; break;
            case AlignH::RIGHT: attr += u" justify=\"right\""; break;
            case AlignH::JUSTIFY: attr += u" justify=\"justify\""; break;
        }
    }
    // set the default words format
    const MStyle &style = score_->style();
    const String mtf = style.style_st(Sid::MUSICAL_TEXT_FONT);
    const CharFormat def_fmt = format_for_words(style);
    // write formatted
    MScoreTextToMusicXml mttm(u"rehearsal", attr, def_fmt, mtf);
    mttm.write_text_fragments(rmk->fragment_list(), xml_);
    xml_.end_element();
    const int offset = calculate_time_delta_in_divisions(rmk->tick(), tick(), div_);
    if (offset) {
        xml_.tag("offset", offset);
    }
    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   harp pedal
//---------------------------------------------------------

void ExportMusicXml::harp_pedals(HarpPedalDiagram const *const hpd, staff_idx_t staff) {
    direction_tag(configuration_, xml_, attr_, hpd);
    xml_.start_element("direction-type");
    XmlWriter::Attributes harp_pedal_attrs;
    add_color_attr(hpd, harp_pedal_attrs);
    if (hpd->is_diagram()) {
        xml_.start_element("harp-pedals", harp_pedal_attrs);
        const std::vector<String> pedal_steps = {u"D", u"C", u"B", u"E", u"F", u"G", u"A"};
        for (size_t idx = 0; idx < pedal_steps.size(); idx++) {
            xml_.start_element("pedal-tuning");
            xml_.tag("pedal-step", pedal_steps.at(idx));
            xml_.tag("pedal-alter", static_cast<int>(hpd->get_pedal_state().at(idx)) - 1);
            xml_.end_element();
        }
        xml_.end_element();
    } else {
        xml_.tag("words", harp_pedal_attrs, hpd->plain_text());
    }
    xml_.end_element();
    const int offset = calculate_time_delta_in_divisions(hpd->tick(), tick(), div_);
    if (offset) {
        xml_.tag("offset", offset);
    }
    direction_e_tag(xml_, staff);
}

//---------------------------------------------------------
//   findDashes -- get index of hairpin in dashes table
//   return -1 if not found
//---------------------------------------------------------

int ExportMusicXml::find_dashes(const TextLineBase *hp) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (dashes_[i] == hp) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   findHairpin -- get index of hairpin in hairpin table
//   return -1 if not found
//---------------------------------------------------------

int ExportMusicXml::find_hairpin(const Hairpin *hp) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (hairpins_[i] == hp) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   findInString
//---------------------------------------------------------

// find the longest first match of dynList's dynamic text in s
// used by the MusicXML export to correctly export dynamics embedded
// in spanner begin- or endtexts
// return match's position and length and the dynamic type

static size_t find_dynamic_in_string(const String &s, size_t &length, String &type) {
    length = 0;
    type.clear();
    size_t match_index = muse::nidx;
    const int n = static_cast<int>(DynamicType::LAST) - 1;

    // for all dynamics, find their text in s
    for (int i = 0; i < n; ++i) {
        DynamicType t = static_cast<DynamicType>(i);
        const String dynamic_text = Dynamic::dynamic_text(t);
        const size_t dynamic_length = dynamic_text.size();
        // note: skip entries with empty text
        if (dynamic_length > 0) {
            const size_t index = s.index_of(dynamic_text);
            if (index != muse::nidx) {
                // found a match, accept it if
                // - it is the first one
                // - or it starts a the same index but is longer ("pp" versus "p")
                if (match_index == muse::nidx || (index == match_index && dynamic_length > length)) {
                    match_index = index;
                    length = dynamic_length;
                    type = String::from_ascii(TConv::to_xml(t).ascii());
                }
            }
        }
    }

    return match_index;
}

//---------------------------------------------------------
//   writeHairpinText
//---------------------------------------------------------

static void write_hairpin_text(XmlWriter &xml, const TextLineBase *const tlb, bool is_start = true) {
    String text = is_start ? tlb->begin_text() : tlb->end_text();
    while (!text.empty()) {
        size_t dynamic_length = 0;
        String dynamics_type;
        size_t dynamic_position = find_dynamic_in_string(text, dynamic_length, dynamics_type);
        if (dynamic_position == muse::nidx || dynamic_position > 0) {
            // text remaining and either no dynamic of not at front of text
            xml.start_element("direction-type");
            String tag = u"words";
            tag +=
                String(u" font-family=\"%1\"").arg(tlb->get_property(is_start ? Pid::BEGIN_FONT_FACE : Pid::END_FONT_FACE).value<String>());
            tag += String(u" font-size=\"%1\"").arg(tlb->get_property(is_start ? Pid::BEGIN_FONT_SIZE : Pid::END_FONT_SIZE).to_real());
            tag += font_style_to_xml(
                static_cast<FontStyle>(tlb->get_property(is_start ? Pid::BEGIN_FONT_STYLE : Pid::END_FONT_STYLE).to_int()));
            tag += color2xml(tlb);
            tag += ExportMusicXml::positioning_attributes(tlb, is_start);
            xml.tag_raw(tag, dynamic_position == muse::nidx ? text : text.left(dynamic_position));
            xml.end_element();
            if (dynamic_position == muse::nidx) {
                text.clear();
            } else if (dynamic_position > 0) {
                text.remove(0, dynamic_position);
                dynamic_position = 0;
            }
        }
        if (dynamic_position == 0) {
            // dynamic at front of text
            xml.start_element("direction-type");
            String tag = u"dynamics";
            tag += color2xml(tlb);
            tag += ExportMusicXml::positioning_attributes(tlb, is_start);
            xml.start_element_raw(tag);
            xml.tag_raw(dynamics_type);
            xml.end_element();
            xml.end_element();
            text.remove(0, dynamic_length);
        }
    }
}

//---------------------------------------------------------
//   hairpin
//---------------------------------------------------------

void ExportMusicXml::hairpin(Hairpin const *const hp, staff_idx_t staff, const Fraction &tick) {
    const bool is_line_type = hp->is_line_type();
    const bool is_start = hp->tick() == tick;
    int n;
    if (is_line_type) {
        if (!hp->line_visible()) {
            if ((is_start && hp->begin_text().is_empty()) || (!is_start && hp->end_text().is_empty())) {
                return;
            }
            direction_tag(configuration_, xml_, attr_, hp);
            write_hairpin_text(xml_, hp, is_start);
            direction_e_tag(xml_, staff, calculate_time_delta_in_divisions(tick, tick_, div_));
            return;
        }
        n = find_dashes(hp);
        if (n >= 0) {
            dashes_[n] = nullptr;
        } else {
            n = find_dashes(nullptr);
            if (n >= 0) {
                dashes_[n] = hp;
            } else {
                LOGD("too many overlapping dashes (hp %p staff %zu tick %d)", hp, staff, tick.ticks());
                return;
            }
        }
    } else {
        n = find_hairpin(hp);
        if (n >= 0) {
            hairpins_[n] = nullptr;
        } else {
            n = find_hairpin(nullptr);
            if (n >= 0) {
                hairpins_[n] = hp;
            } else {
                LOGD("too many overlapping hairpins (hp %p staff %zu tick %d)", hp, staff, tick.ticks());
                return;
            }
        }
    }

    direction_tag(configuration_, xml_, attr_, hp);
    if (is_start) {
        write_hairpin_text(xml_, hp, is_start);
    }
    if (is_line_type) {
        if (hp->line_visible()) {
            if (is_start) {
                xml_.start_element("direction-type");
                String tag = u"dashes type=\"start\"";
                tag += String(u" number=\"%1\"").arg(n + 1);
                tag += color2xml(hp);
                tag += positioning_attributes(hp, is_start);
                xml_.tag_raw(tag);
                xml_.end_element();
            } else {
                xml_.start_element("direction-type");
                xml_.tag_raw(String(u"dashes type=\"stop\" number=\"%1\"").arg(n + 1));
                xml_.end_element();
            }
        }
    } else {
        xml_.start_element("direction-type");
        String tag = u"wedge type=";
        if (is_start) {
            if (hp->hairpin_type() == HairpinType::CRESC_HAIRPIN) {
                tag += u"\"crescendo\"";
                if (hp->hairpin_circled_tip()) {
                    tag += u" niente=\"yes\"";
                }
            } else {
                tag += u"\"diminuendo\"";
            }
            tag += color2xml(hp);
            tag += positioning_attributes(hp, is_start);
            switch (hp->line_style()) {
                case LineType::DASHED: tag += u" line-type=\"dashed\""; break;
                case LineType::DOTTED: tag += u" line-type=\"dotted\""; break;
                case LineType::SOLID:
                default: break;
            }
            if (configuration()->export_layout() && (hp->line_style() == LineType::DASHED)) {
                tag += String(u" dash-length=\"%1\"").arg(String::number(hp->dash_line_len() * 10, 2));
                tag += String(u" space-length=\"%1\"").arg(String::number(hp->dash_gap_len() * 10, 2));
            }
            if (configuration()->export_layout() && hp->hairpin_type() == HairpinType::CRESC_HAIRPIN) {
                tag += String(u" spread=\"%1\"").arg(String::number(hp->hairpin_height().val() * 10, 2));
            }
        } else {
            tag += u"\"stop\"";
            if (hp->hairpin_circled_tip() && hp->hairpin_type() == HairpinType::DIM_HAIRPIN) {
                tag += u" niente=\"yes\"";
            }
            if (configuration()->export_layout() && hp->hairpin_type() == HairpinType::DIM_HAIRPIN) {
                tag += String(u" spread=\"%1\"").arg(String::number(hp->hairpin_height().val() * 10, 2));
            }
        }
        tag += String(u" number=\"%1\"").arg(n + 1);
        xml_.tag_raw(tag);
        xml_.end_element();
    }
    if (!is_start) {
        write_hairpin_text(xml_, hp, is_start);
    }
    direction_e_tag(xml_, staff, calculate_time_delta_in_divisions(is_start ? hp->tick() : hp->tick2(), tick_, div_));
}

//---------------------------------------------------------
//   findOttava -- get index of ottava in ottava table
//   return -1 if not found
//---------------------------------------------------------

int ExportMusicXml::find_ottava(const Ottava *ot) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (ottavas_[i] == ot) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   ottava
// <octave-shift type="down" size="8" relative-y="14"/>
// <octave-shift type="stop" size="8"/>
//---------------------------------------------------------

void ExportMusicXml::ottava(Ottava const *const ot, staff_idx_t staff, const Fraction &tick) {
    int n = find_ottava(ot);
    bool is_start = ot->tick() == tick;
    const Measure *measure = ot->start_element() ? ot->start_element()->find_measure() : nullptr;
    const Fraction measure_start = measure ? measure->tick() : Fraction(0, 1);
    if (n >= 0) {
        ottavas_[n] = 0;
    } else {
        n = find_ottava(0);
        if (n >= 0) {
            ottavas_[n] = ot;
        } else {
            LOGD("too many overlapping ottavas (ot %p staff %zu tick %d)", ot, staff, tick.ticks());
            return;
        }
    }

    String octave_shift_xml;
    const OttavaType st = ot->ottava_type();
    if (ot->tick() == tick) {
        String sz;
        String tp;
        switch (st) {
            case OttavaType::OTTAVA_8VA:
                sz = u"8";
                tp = u"down";
                break;
            case OttavaType::OTTAVA_15MA:
                sz = u"15";
                tp = u"down";
                break;
            case OttavaType::OTTAVA_22MA:
                sz = u"22";
                tp = u"down";
                break;
            case OttavaType::OTTAVA_8VB:
                sz = u"8";
                tp = u"up";
                break;
            case OttavaType::OTTAVA_15MB:
                sz = u"15";
                tp = u"up";
                break;
            case OttavaType::OTTAVA_22MB:
                sz = u"22";
                tp = u"up";
                break;
            default: LOGD("ottava subtype %d not understood", int(st));
        }
        if (!sz.empty() && !tp.empty()) {
            octave_shift_xml = String(u"octave-shift type=\"%1\" size=\"%2\" number=\"%3\"").arg(tp, sz).arg(n + 1);
        }
    } else {
        if (st == OttavaType::OTTAVA_8VA || st == OttavaType::OTTAVA_8VB) {
            octave_shift_xml = String(u"octave-shift type=\"stop\" size=\"8\" number=\"%1\"").arg(n + 1);
        } else if (st == OttavaType::OTTAVA_15MA || st == OttavaType::OTTAVA_15MB) {
            octave_shift_xml = String(u"octave-shift type=\"stop\" size=\"15\" number=\"%1\"").arg(n + 1);
        } else if (st == OttavaType::OTTAVA_22MA || st == OttavaType::OTTAVA_22MB) {
            octave_shift_xml = String(u"octave-shift type=\"stop\" size=\"22\" number=\"%1\"").arg(n + 1);
        } else {
            LOGD("ottava subtype %d not understood", int(st));
        }
    }

    if (!octave_shift_xml.empty()) {
        // generate backup or forward to the start time of the element
        const Fraction tick_to_write = is_start ? ot->tick() : ot->tick2();
        move_to_tick_if_need(tick_to_write, ot->track(), measure_start);

        direction_tag(configuration_, xml_, attr_, nullptr);
        xml_.start_element("direction-type");
        octave_shift_xml += color2xml(ot);
        octave_shift_xml += positioning_attributes(ot, ot->tick() == tick);
        xml_.tag_raw(octave_shift_xml);
        xml_.end_element();
        direction_e_tag(xml_, staff);
    }
}

//---------------------------------------------------------
//   pedal
//---------------------------------------------------------

void ExportMusicXml::pedal(Pedal const *const pd, staff_idx_t staff, const Fraction &tick) {
    // "change" type is handled only on the beginning of pedal lines
    if (pd->tick() != tick && pd->end_hook_type() == HookType::HOOK_45) {
        return;
    }
    bool is_start = pd->tick() == tick;

    direction_tag(configuration_, xml_, attr_, pd);
    xml_.start_element("direction-type");
    String pedal_type;
    String pedal_xml;
    String sign_text;
    String line_text = pd->line_visible() ? u" line=\"yes\"" : u" line=\"no\"";
    if (pd->tick() == tick) {
        switch (pd->begin_hook_type()) {
            case HookType::HOOK_45: pedal_type = u"change"; break;
            case HookType::NONE: pedal_type = (pd->line_visible() && pd->begin_text().is_empty()) ? u"resume" : u"start"; break;
            default: pedal_type = u"start";
        }
        sign_text = pd->begin_text().is_empty() ? u" sign=\"no\"" : u" sign=\"yes\"";
        if (pd->begin_text() == u"<sym>keyboardPedalSost</sym>" || pd->begin_text() == u"<sym>keyboardPedalS</sym>") {
            pedal_type = u"sostenuto";
        }
    } else {
        switch (pd->end_hook_type()) {
            case HookType::NONE: pedal_type = (pd->line_visible() && pd->end_text().is_empty()) ? u"discontinue" : u"stop"; break;
            default: pedal_type = u"stop";
        }
        // "change" type is handled only on the beginning of pedal lines

        sign_text = pd->end_text().is_empty() ? u" sign=\"no\"" : u" sign=\"yes\"";
    }
    pedal_xml = String(u"pedal type=\"%1\"").arg(pedal_type);
    pedal_xml += line_text;
    pedal_xml += sign_text;
    pedal_xml += color2xml(pd);
    pedal_xml += positioning_attributes(pd, pd->tick() == tick);
    xml_.tag_raw(pedal_xml);
    xml_.end_element();
    direction_e_tag(xml_, staff, calculate_time_delta_in_divisions(is_start ? pd->tick() : pd->tick2(), tick_, div_));
}

//---------------------------------------------------------
//   findBracket -- get index of bracket in bracket table
//   return -1 if not found
//---------------------------------------------------------

int ExportMusicXml::find_bracket(const TextLineBase *tl) const {
    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        if (brackets_[i] == tl) {
            return i;
        }
    }
    return -1;
}

//---------------------------------------------------------
//   textLine
//---------------------------------------------------------

void ExportMusicXml::text_line(TextLineBase const *const tl, staff_idx_t staff, const Fraction &tick) {
    using namespace muse::draw;

    bool is_start = tl->tick() == tick;
    if (!tl->line_visible()) {
        if ((is_start && tl->begin_text().is_empty()) || (!is_start && tl->end_text().is_empty())) {
            return;
        }
        direction_tag(configuration_, xml_, attr_, tl);
        write_hairpin_text(xml_, tl, is_start);
        direction_e_tag(xml_, staff, calculate_time_delta_in_divisions(tick, tick_, div_));
        return;
    }

    int n;
    // special case: a dashed line w/o hooks is written as dashes
    const bool is_dashes =
        tl->line_style() == LineType::DASHED && (tl->begin_hook_type() == HookType::NONE) && (tl->end_hook_type() == HookType::NONE);

    if (is_dashes) {
        n = find_dashes(tl);
        if (n >= 0) {
            dashes_[n] = nullptr;
        } else {
            n = find_bracket(nullptr);
            if (n >= 0) {
                dashes_[n] = tl;
            } else {
                LOGD("too many overlapping dashes (tl %p staff %zu tick %d)", tl, staff, tick.ticks());
                return;
            }
        }
    } else {
        n = find_bracket(tl);
        if (n >= 0) {
            brackets_[n] = nullptr;
        } else {
            n = find_bracket(nullptr);
            if (n >= 0) {
                brackets_[n] = tl;
            } else {
                LOGD("too many overlapping textlines (tl %p staff %zu tick %d)", tl, staff, tick.ticks());
                return;
            }
        }
    }

    String rest;
    PointF p;

    String type;
    HookType hook_type = HookType::NONE;
    double hook_height = 0.0;
    if (tl->tick() == tick) {
        if (!is_dashes) {
            String line_type;
            switch (tl->line_style()) {
                case LineType::SOLID: line_type = u"solid"; break;
                case LineType::DASHED: line_type = u"dashed"; break;
                case LineType::DOTTED: line_type = u"dotted"; break;
            }
            rest += String(u" line-type=\"%1\"").arg(line_type);
        }
        hook_type = tl->begin_hook_type();
        hook_height = tl->begin_hook_height().val();
        if (!tl->segments_empty()) {
            p = tl->front_segment()->offset();
        }
        type = u"start";
    } else {
        hook_type = tl->end_hook_type();
        hook_height = tl->end_hook_height().val();
        if (!tl->segments_empty()) {
            p = (to_line_segment(tl->back_segment()))->user_off2();
        }
        type = u"stop";
    }

    String line_end;
    switch (hook_type) {
        case HookType::HOOK_90:
            line_end = (tl->placement() == PlacementV::BELOW) ? u"up" : u"down";
            rest += String(u" end-length=\"%1\"").arg(std::abs(hook_height * 10));
            break;
        case HookType::HOOK_90T:
            line_end = u"both";
            rest += String(u" end-length=\"%1\"").arg(std::abs(hook_height * 20));
            break;
        case HookType::ARROW:
        case HookType::ARROW_FILLED: line_end = u"arrow"; break;
        case HookType::NONE: line_end = u"none"; break;
        default: line_end = u"none"; LOGD("HookType %d not supported", int(hook_type));
    }

    rest += color2xml(tl);
    rest += positioning_attributes(tl, tl->tick() == tick);

    direction_tag(configuration_, xml_, attr_, tl);

    if (!tl->begin_text().is_empty() && tl->tick() == tick) {
        xml_.start_element("direction-type");
        xml_.tag("words", tl->begin_text());
        xml_.end_element();
    }

    if (tl->line_visible()) {
        xml_.start_element("direction-type");
        if (is_dashes) {
            xml_.tag("dashes", {{"type", type}, {"number", n + 1}});
        } else {
            xml_.tag_raw(String(u"bracket type=\"%1\" number=\"%2\" line-end=\"%3\"%4").arg(type, String::number(n + 1), line_end, rest));
        }
        xml_.end_element();
    }

    if (!tl->end_text().is_empty() && tl->tick() != tick) {
        xml_.start_element("direction-type");
        xml_.tag("words", tl->end_text());
        xml_.end_element();
    }

    direction_e_tag(xml_, staff, calculate_time_delta_in_divisions(is_start ? tl->tick() : tl->tick2(), tick_, div_));
}

//---------------------------------------------------------
//   writeWordsAndSymbolsXml
//   Writes a string that may contain <sym>...</sym> elements as words and symbol elements.
//---------------------------------------------------------

static void write_words_and_symbols_xml(mu::engraving::XmlWriter &xml, const mu::engraving::String &words,
                                        const mu::engraving::String &attrs) {
    // Write symbols as separate elements, the rest as words.
    static const std::wregex sym_regex(LR"(<sym>([^<>]+)</sym>)");
    std::wstring wwords = words.to_std_w_string();
    std::wsregex_iterator it(wwords.begin(), wwords.end(), sym_regex);
    std::wsregex_iterator end;
    size_t last_pos = 0;
    bool found_sym = false;
    while (it != end) {
        found_sym = true;
        size_t match_pos = it->position();
        size_t match_len = it->length();
        // Text before <sym>
        if (match_pos > last_pos) {
            mu::engraving::String before = mu::engraving::String::from_std_w_string(wwords.substr(last_pos, match_pos - last_pos));
            if (!before.trimmed().is_empty()) {
                xml.tag_raw(u"words" + attrs, before.trimmed());
            }
        }
        mu::engraving::String sym_name = mu::engraving::String::from_std_w_string((*it)[1].str());
        if (!sym_name.trimmed().is_empty()) {
            xml.tag_raw(u"symbol" + attrs, sym_name.trimmed());
        }
        last_pos = match_pos + match_len;
        ++it;
    }
    // Text after last <sym>
    if (found_sym && last_pos < wwords.size()) {
        mu::engraving::String after = mu::engraving::String::from_std_w_string(wwords.substr(last_pos));
        if (!after.trimmed().is_empty()) {
            xml.tag_raw(u"words" + attrs, after.trimmed());
        }
    }
    // If no <sym> found
    if (!found_sym) {
        xml.tag_raw(u"words" + attrs, words);
    }
}

//---------------------------------------------------------
//   dynamic
//---------------------------------------------------------

// In MuseScore dynamics are essentially user-defined texts, therefore the ones
// supported by MusicXML need to be filtered out. Everything not recognized
// as MusicXML dynamics is written as other-dynamics.

template <typename T>
inline std::set<String> &operator<<(std::set<String> &s, const T &v) {
    s.emplace(v);
    return s;
}

static String single_dynamic_smufl(const String &text) {
    if (!text.starts_with(u"<sym>") || !text.ends_with(u"</sym>")) {
        return {};
    }
    const String name = text.mid(5, text.size() - 11);
    const SymId id = SymNames::sym_id_by_name(name);
    // Font extensions and engine-local symbols are not SMuFL standard names.
    if (id == SymId::NO_SYM || id >= SymId::NOTEHEAD_DOUBLE_WHOLE_ALT) {
        return {};
    }
    const String canonical = String::from_ascii(SymNames::name_for_sym_id(id).ascii());
    return canonical == name ? canonical : String();
}

static void write_dynamic_other_text(XmlWriter &xml, const String &value) {
    if (value.empty()) {
        return;
    }
    const bool whitespace = value.trimmed().empty();
    if (!whitespace && !value.contains(u'\r')) {
        xml.tag("other-dynamics", value);
        return;
    }
    // References keep whitespace-only nodes visible to the importer and
    // prevent XML end-of-line normalization from changing carriage returns.
    String raw;
    for (size_t i = 0; i < value.size(); ++i) {
        const char16_t c = value.at(i).unicode();
        raw += whitespace || c == u'\r' ? String(u"&#%1;").arg(static_cast<int>(c)) : String::to_xml_escaped(c);
    }
    xml.write_xml(u"other-dynamics", raw);
}

// Preserve named SMuFL symbols in grouped dynamics, including plain text around
// them. Formatted or non-standard symbols still use the existing fallback.
static bool write_grouped_dynamic_smufl(XmlWriter &xml, const Dynamic *source) {
    const String text = source->xml_text();
    static const std::map<String, String> letter_symbols{
        {u"dynamicPiano", u"p"},     {u"dynamicMezzo", u"m"}, {u"dynamicForte", u"f"}, {u"dynamicRinforzando", u"r"},
        {u"dynamicSforzando", u"s"}, {u"dynamicZ", u"z"},     {u"dynamicNiente", u"n"}};
    std::vector<std::pair<String, bool>> parts;
    bool has_named_symbol = false;
    size_t cursor = 0;
    while (cursor < text.size()) {
        if (text.mid(cursor, 5) == u"<sym>") {
            const size_t end = text.index_of(u"</sym>", cursor + 5);
            if (end == muse::nidx) {
                return false;
            }
            const String name = single_dynamic_smufl(text.mid(cursor, end + 6 - cursor));
            if (name.empty()) {
                return false;
            }
            has_named_symbol = has_named_symbol || !muse::contains(letter_symbols, name);
            parts.emplace_back(name, true);
            cursor = end + 6;
        } else {
            const size_t next = text.index_of(u"<sym>", cursor);
            const String literal = text.mid(cursor, next == muse::nidx ? muse::nidx : next - cursor);
            if (literal.contains(u'<') || literal.contains(u'>')) {
                return false;
            }
            // Sentinels keep whitespace-only fragments from being discarded by
            // the generic XML reader while entities are decoded.
            String encoded_literal = literal;
            encoded_literal.replace(u"\r", u"&#13;");
            XmlStreamReader reader((String(u"<text>X") + encoded_literal + u"X</text>").to_utf8());
            String decoded;
            while (!reader.at_end()) {
                reader.read_next();
                if (reader.is_characters()) {
                    decoded += reader.text();
                }
            }
            if (reader.is_error() || decoded.size() < 2) {
                return false;
            }
            parts.emplace_back(decoded.mid(1, decoded.size() - 2), false);
            cursor = next == muse::nidx ? text.size() : next;
        }
    }
    // Distinct source representations can have the same rendered text but
    // different dynamic types. Combine a prose run only when the existing
    // classifier verifies both the type and the exact resulting text.
    size_t merge_begin = muse::nidx;
    size_t merge_end = muse::nidx;
    String merged_text;
    Dynamic probe(source->score()->dummy());
    probe.set_dynamic_type(text);
    bool safe_to_probe = true;
    bool has_whitespace_part = false;
    for (const auto &[part, is_symbol] : parts) {
        if (is_symbol) {
            continue;
        }
        has_whitespace_part = has_whitespace_part || (!part.empty() && part.trimmed().empty());
        if (part.contains(u'<') || part.contains(u'>')) {
            safe_to_probe = false;
            continue;
        }
        bool only_letters = !part.empty();
        for (size_t k = 0; k < part.size() && only_letters; ++k) {
            const char16_t c = part.at(k).unicode();
            only_letters = c == u'p' || c == u'm' || c == u'f' || c == u'r' || c == u's' || c == u'z' || c == u'n';
        }
        if (only_letters) {
            safe_to_probe = false;
        }
    }
    if (safe_to_probe && source->dynamic_type() != DynamicType::OTHER && probe.dynamic_type() != source->dynamic_type()) {
        size_t begin = 0;
        while (begin < parts.size()) {
            if (parts[begin].second && !muse::contains(letter_symbols, parts[begin].first)) {
                ++begin;
                continue;
            }
            size_t end = begin;
            bool has_text = false;
            bool previous_symbol = false;
            size_t symbol_runs = 0;
            String glyphs;
            String combined;
            size_t candidate_end = muse::nidx;
            while (end < parts.size()) {
                const auto &[part, is_symbol] = parts[end];
                if (is_symbol && !muse::contains(letter_symbols, part)) {
                    break;
                }
                has_text = has_text || (!is_symbol && !part.empty());
                if (is_symbol) {
                    symbol_runs += !previous_symbol;
                    glyphs += u"<sym>" + part + u"</sym>";
                    combined += letter_symbols.at(part);
                } else {
                    combined += part;
                }
                previous_symbol = is_symbol;
                ++end;
                if (has_text && is_symbol && symbol_runs == 1 && glyphs == Dynamic::dynamic_text(source->dynamic_type())) {
                    candidate_end = end;
                    break;
                }
            }
            if (candidate_end != muse::nidx) {
                String candidate;
                for (size_t j = 0; j < parts.size();) {
                    if (j == begin) {
                        candidate += TextBase::plain_to_xml_text(combined);
                        j = candidate_end;
                    } else {
                        candidate += parts[j].second ? u"<sym>" + parts[j].first + u"</sym>" : TextBase::plain_to_xml_text(parts[j].first);
                        ++j;
                    }
                }
                probe.set_dynamic_type(candidate);
                if (probe.dynamic_type() == source->dynamic_type() && probe.xml_text() == text) {
                    merge_begin = begin;
                    merge_end = candidate_end;
                    merged_text = combined;
                }
                break;
            }
            begin = end;
        }
    }
    if (!has_named_symbol && merge_begin == muse::nidx && !has_whitespace_part) {
        return false;
    }
    size_t i = 0;
    while (i < parts.size()) {
        if (i == merge_begin) {
            write_dynamic_other_text(xml, merged_text);
            i = merge_end;
            continue;
        }
        const auto &[value, symbol] = parts[i];
        if (!symbol) {
            write_dynamic_other_text(xml, value);
        } else if (!muse::contains(letter_symbols, value)) {
            xml.tag_raw(u"other-dynamics smufl=\"" + value + u"\"");
        } else {
            const String &letter = letter_symbols.at(value);
            if (letter == u"p" || letter == u"f" || letter == u"n") {
                xml.tag_raw(letter);
            } else {
                xml.tag("other-dynamics", letter);
            }
        }
        ++i;
    }
    return true;
}

void ExportMusicXml::dynamic(Dynamic const *const dyn, staff_idx_t staff) {
    static const std::set<String> valid_music_xml_dynamics{
        u"p",  u"pp", u"ppp", u"pppp", u"ppppp", u"pppppp", u"f",   u"ff",  u"fff",  u"ffff", u"fffff", u"ffffff", u"mp",
        u"mf", u"sf", u"sfp", u"sfpp", u"fp",    u"rf",     u"rfz", u"sfz", u"sffz", u"fz",   u"n",     u"pf",     u"sfzp"};

    direction_tag(configuration_, xml_, attr_, dyn);

    xml_.start_element("direction-type");

    String tag_name = u"dynamics";
    tag_name += frame2xml(dyn);
    tag_name += color2xml(dyn);
    tag_name += positioning_attributes(dyn);
    xml_.start_element_raw(tag_name);
    const String dyn_type_name = String::from_ascii(TConv::to_xml(dyn->dynamic_type()).ascii());
    bool has_custom_text = dyn->has_custom_text();

    if (muse::contains(valid_music_xml_dynamics, dyn_type_name) && !has_custom_text) {
        xml_.tag_raw(dyn_type_name);
    } else if (!has_custom_text && (dyn_type_name == u"m" || dyn_type_name == u"r" || dyn_type_name == u"s" || dyn_type_name == u"z")) {
        xml_.tag("other-dynamics", dyn_type_name);
    } else if (const String smufl = single_dynamic_smufl(dyn->xml_text()); !smufl.empty()) {
        xml_.tag_raw(u"other-dynamics smufl=\"" + smufl + u"\"");
    } else if (write_grouped_dynamic_smufl(xml_, dyn)) {
        // Already written as a single MusicXML dynamics container.
    } else if (!dyn_type_name.empty()) {
        static const std::map<unsigned short, Char> map = {{0xE520, u'p'}, {0xE521, u'm'}, {0xE522, u'f'}, {0xE523, u'r'},
                                                           {0xE524, u's'}, {0xE525, u'z'}, {0xE526, u'n'}};

        String dyn_text = dyn_type_name;
        if (dyn->dynamic_type() == DynamicType::OTHER || has_custom_text) {
            dyn_text = dyn->plain_text();
        }

        // collect consecutive runs of either dynamics glyphs
        // or other characters and write the runs.
        String text;
        bool in_dynamics_sym = false;
        for (size_t i = 0; i < dyn_text.size(); ++i) {
            Char ch = dyn_text.at(i);
            const auto it = map.find(ch.unicode());
            if (it != map.end()) {
                // found a SMuFL single letter dynamics glyph
                if (!in_dynamics_sym) {
                    if (!text.empty()) {
                        xml_.tag("other-dynamics", text);
                        text.clear();
                    }
                    in_dynamics_sym = true;
                }
                text += it->second;
            } else {
                // found a non-dynamics character
                if (in_dynamics_sym) {
                    if (!text.empty()) {
                        if (muse::contains(valid_music_xml_dynamics, text)) {
                            xml_.tag_raw(text);
                        } else {
                            // TODO: this is wrong, should be <words>
                            xml_.tag("other-dynamics", text);
                        }
                        text.clear();
                    }
                    in_dynamics_sym = false;
                }
                text += ch.unicode();
            }
        }
        if (!text.empty()) {
            if (in_dynamics_sym && muse::contains(valid_music_xml_dynamics, text)) {
                xml_.tag_raw(text);
            } else {
                xml_.tag("other-dynamics", text);
            }
        }
    }

    xml_.end_element();

    xml_.end_element();

    const int offset = calculate_time_delta_in_divisions(dyn->tick(), tick(), div_);
    if (offset) {
        xml_.tag("offset", offset);
    }

    if (staff) {
        xml_.tag("staff", static_cast<int>(staff));
    }

    if (dyn->velocity() > 0) {
        xml_.tag_raw(String(u"sound dynamics=\"%1\"").arg(String::number(dyn->velocity() * 100.0 / 90.0, 2)));
    }

    xml_.end_element();
}

//---------------------------------------------------------
//   lyrics
//---------------------------------------------------------

void ExportMusicXml::lyrics(const std::vector<Lyrics *> &ll, const track_idx_t trk) {
    for (const Lyrics *l : ll) {
        if (l && !l->xml_text().is_empty()) {
            if ((l)->track() == trk) {
                String lyric_xml = String(u"lyric number=\"%1\"").arg((l)->verse() + 1);
                lyric_xml += color2xml(l);
                lyric_xml += positioning_attributes(l);
                if (!l->visible()) {
                    lyric_xml += u" print-object=\"no\"";
                }
                if (l->place_above()) {
                    lyric_xml += u" placement=\"above\"";
                }
                xml_.start_element_raw(lyric_xml);
                LyricsSyllabic syl = (l)->syllabic();
                String s;
                switch (syl) {
                    case LyricsSyllabic::SINGLE: s = u"single"; break;
                    case LyricsSyllabic::BEGIN: s = u"begin"; break;
                    case LyricsSyllabic::END: s = u"end"; break;
                    case LyricsSyllabic::MIDDLE: s = u"middle"; break;
                    default: LOGD("unknown syllabic %d", int(syl));
                }
                xml_.tag("syllabic", s);
                String attr; // TODO TBD
                // set the default words format
                const String mtf = score_->style().style_st(Sid::MUSICAL_TEXT_FONT);
                CharFormat def_fmt;
                def_fmt.set_font_family(score_->style().style_st(l->is_even() ? Sid::LYRICS_EVEN_FONT_FACE : Sid::LYRICS_ODD_FONT_FACE));
                def_fmt.set_font_size(score_->style().style_d(l->is_even() ? Sid::LYRICS_EVEN_FONT_SIZE : Sid::LYRICS_ODD_FONT_SIZE));
                // write formatted
                MScoreTextToMusicXml mttm(u"text", attr, def_fmt, mtf);
                mttm.write_text_fragments(l->fragment_list(), xml_);
                if (l->ticks().is_not_zero()) {
                    xml_.tag("extend");
                }
                xml_.end_element();
            }
        }
    }
}

//---------------------------------------------------------
//   directionJump -- write jump
//---------------------------------------------------------

// LVIFIX: TODO coda and segno should be numbered uniquely

static void direction_jump(XmlWriter &xml, const Jump *const jp) {
    JumpType jtp = jp->jump_type();
    String words;
    String sound;
    bool is_da_capo = false;
    bool is_dal_segno = false;
    if (jtp == JumpType::DC) {
        if (jp->xml_text().empty()) {
            words = u"D.C.";
        } else {
            words = jp->xml_text();
        }
        is_da_capo = true;
    } else if (jtp == JumpType::DC_AL_FINE) {
        if (jp->xml_text().empty()) {
            words = u"D.C. al Fine";
        } else {
            words = jp->xml_text();
        }
        is_da_capo = true;
    } else if (jtp == JumpType::DC_AL_CODA) {
        if (jp->xml_text().empty()) {
            words = u"D.C. al Coda";
        } else {
            words = jp->xml_text();
        }
        is_da_capo = true;
    } else if (jtp == JumpType::DS_AL_CODA) {
        if (jp->xml_text().empty()) {
            words = u"D.S. al Coda";
        } else {
            words = jp->xml_text();
        }
        is_dal_segno = true;
    } else if (jtp == JumpType::DS_AL_FINE) {
        if (jp->xml_text().empty()) {
            words = u"D.S. al Fine";
        } else {
            words = jp->xml_text();
        }
        is_dal_segno = true;
    } else if (jtp == JumpType::DS) {
        words = u"D.S.";
        is_dal_segno = true;
    } else {
        words = jp->xml_text();
        if (jp->jump_to() == "start") {
            is_da_capo = true;
        } else {
            is_dal_segno = true;
        }
    }

    if (is_da_capo) {
        sound = u"dacapo=\"yes\"";
    } else if (is_dal_segno) {
        if (jp->xml_text().empty()) {
            sound = u"dalsegno=\"1\"";
        } else {
            sound = u"dalsegno=\"" + jp->jump_to() + u"\"";
        }
    }

    if (ExportMusicXml::can_write(jp) && !words.empty()) {
        xml.start_element("direction", {{"placement", TConv::to_xml(jp->placement())}});
        xml.start_element("direction-type");
        String attrs = color2xml(jp);
        attrs += ExportMusicXml::positioning_attributes(jp);
        write_words_and_symbols_xml(xml, words, attrs);
        xml.end_element();
        if (!sound.empty()) {
            xml.tag_raw(u"sound " + sound);
        }
        xml.end_element();
    } else if (!sound.empty()) {
        if (!sound.empty()) {
            xml.tag_raw(u"sound " + sound);
        }
    }
}

//---------------------------------------------------------
//   getEffectiveMarkerType
//---------------------------------------------------------

static MarkerType get_effective_marker_type(const Marker *const m, const std::vector<const Jump *> &jumps) {
    MarkerType mtp = m->marker_type();

    if (mtp != MarkerType::USER) {
        return mtp;
    }

    // Try to guess marker type from its usage in jumps.
    const String label = m->label();

    for (const Jump *j : jumps) {
        MarkerType guessed_marker_type = mtp;

        if (j->jump_to() == label) {
            guessed_marker_type = MarkerType::SEGNO;
        } else if (j->play_until() == label) {
            guessed_marker_type = j->continue_at().is_empty() ? MarkerType::FINE : MarkerType::TOCODA;
        } else if (j->continue_at() == label) {
            guessed_marker_type = MarkerType::CODA;
        }

        if (guessed_marker_type != mtp) {
            if (mtp != MarkerType::USER) {
                // Type guesses differ for different jump elements.
                LOGD("Cannot guess type for marker with label=\"%s\"", muPrintable(label));
                return MarkerType::USER;
            }
            mtp = guessed_marker_type;
        }
    }

    return mtp;
}

//---------------------------------------------------------
//   findCodaLabel
//---------------------------------------------------------

static String find_coda_label(const std::vector<const Jump *> &jumps, const String &to_coda_label) {
    for (const Jump *j : jumps) {
        if (j->play_until() == to_coda_label) {
            return j->continue_at();
        }
    }

    return String();
}

//---------------------------------------------------------
//   directionMarker -- write marker
//---------------------------------------------------------

static void direction_marker(XmlWriter &xml, const Marker *const m, const std::vector<const Jump *> &jumps) {
    const MarkerType mtp = get_effective_marker_type(m, jumps);
    String words;
    String smufl;
    String type;
    String sound;

    switch (mtp) {
        case MarkerType::VARCODA: smufl = u"codaSquare"; [[fallthrough]];
        case MarkerType::CODA:
        case MarkerType::CODETTA:
            type = u"coda";
            if (m->label().empty()) {
                sound = u"coda=\"1\"";
            } else {
                sound = u"coda=\"" + m->label() + u"\"";
            }
            break;
        case MarkerType::VARSEGNO: smufl = u"segnoSerpent1"; [[fallthrough]];
        case MarkerType::SEGNO:
            type = u"segno";
            if (m->label().empty()) {
                sound = u"segno=\"1\"";
            } else {
                sound = u"segno=\"" + m->label() + u"\"";
            }
            break;
        case MarkerType::FINE:
            words = m->xml_text();
            sound = u"fine=\"yes\"";
            break;
        case MarkerType::TOCODA:
        case MarkerType::TOCODASYM:
        case MarkerType::DA_CODA:
        case MarkerType::DA_DBLCODA: {
            if (m->xml_text().empty()) {
                words = u"To Coda";
            } else {
                words = m->xml_text();
            }
            const String coda_label = find_coda_label(jumps, m->label());
            if (coda_label.empty()) {
                sound = u"tocoda=\"1\"";
            } else {
                sound = u"tocoda=\"" + coda_label + u"\"";
            }
            break;
        }
        case MarkerType::USER: LOGD("marker type=%d not implemented", int(mtp)); break;
    }

    if (ExportMusicXml::can_write(m) && (!words.empty() || !type.empty())) {
        xml.start_element("direction", {{"placement", TConv::to_xml(m->placement())}});
        xml.start_element("direction-type");
        String attrs = color2xml(m);
        attrs += ExportMusicXml::positioning_attributes(m);
        if (!smufl.empty()) {
            attrs += String(u" smufl=\"%1\"").arg(smufl);
        }
        if (!type.empty()) {
            xml.tag_raw(type + attrs);
        }
        if (!words.empty()) {
            write_words_and_symbols_xml(xml, words, attrs);
        }
        xml.end_element();
        if (!sound.empty()) {
            xml.tag_raw(String(u"sound ") + sound);
        }
        xml.end_element();
    } else if (!sound.empty()) {
        xml.tag_raw(String(u"sound ") + sound);
    }
}

//---------------------------------------------------------
//  findTrackForAnnotations
//---------------------------------------------------------

// An annotation is attached to the staff, with track set
// to the lowest track in the staff. Find a track for it
// (the lowest track in this staff that has a chord or rest)

static track_idx_t find_track_for_annotations(track_idx_t track, Segment *seg) {
    if (!(seg->is_chord_rest_type() || seg->is_time_tick_type())) {
        return muse::nidx;
    }

    staff_idx_t staff = track / ::mu::engraving::voices;
    track_idx_t strack = staff * ::mu::engraving::voices;  // start track of staff containing track
    track_idx_t etrack = strack + ::mu::engraving::voices; // end track of staff containing track + 1

    for (track_idx_t i = strack; i < etrack; i++) {
        if (seg->element(i)) {
            return i;
        }
    }

    return muse::nidx;
}

//---------------------------------------------------------
//  repeatAtMeasureStart -- write repeats at begin of measure
//---------------------------------------------------------

void ExportMusicXml::repeat_at_measure_start(Attributes &attr, const Measure *const m, track_idx_t strack, track_idx_t etrack,
                                             track_idx_t track) {
    // loop over all segments
    for (EngravingItem *e : m->el()) {
        track_idx_t wtrack = muse::nidx; // track to write jump
        if (strack <= e->track() && e->track() < etrack) {
            wtrack = find_track_for_annotations(e->track(), m->first(SegmentType::CHORD_REST));
        }
        if (track != wtrack) {
            continue;
        }
        switch (e->type()) {
            case ElementType::MARKER: {
                // filter out the markers at measure Start
                const Marker *const mk = to_marker(e);
                const MarkerType mtp = get_effective_marker_type(mk, jump_elements_);

                switch (mtp) {
                    case MarkerType::SEGNO:
                    case MarkerType::VARSEGNO:
                    case MarkerType::CODA:
                    case MarkerType::VARCODA:
                    case MarkerType::CODETTA:
                        LOGD(" -> handled");
                        attr.do_attr(xml_, false);
                        direction_marker(xml_, mk, jump_elements_);
                        break;
                    case MarkerType::FINE:
                    case MarkerType::TOCODA:
                    case MarkerType::TOCODASYM:
                    case MarkerType::DA_CODA:
                    case MarkerType::DA_DBLCODA:
                        // ignore
                        break;
                    case MarkerType::USER: LOGD("repeatAtMeasureStart: marker %d not implemented", int(mtp)); break;
                }
            } break;
            default: LOGD("repeatAtMeasureStart: direction type %s at tick %d not implemented", e->type_name(), m->tick().ticks()); break;
        }
    }
}

//---------------------------------------------------------
//  repeatAtMeasureStop -- write repeats at end of measure
//---------------------------------------------------------

void ExportMusicXml::repeat_at_measure_stop(const Measure *const m, track_idx_t strack, track_idx_t etrack, track_idx_t track) {
    for (EngravingItem *e : m->el()) {
        track_idx_t wtrack = muse::nidx; // track to write jump
        if (strack <= e->track() && e->track() < etrack) {
            wtrack = find_track_for_annotations(e->track(), m->first(SegmentType::CHORD_REST));
        }
        if (track != wtrack) {
            continue;
        }
        switch (e->type()) {
            case ElementType::MARKER: {
                // filter out the markers at measure stop
                const Marker *const mk = to_marker(e);
                const MarkerType mtp = get_effective_marker_type(mk, jump_elements_);

                switch (mtp) {
                    case MarkerType::FINE:
                    case MarkerType::TOCODA:
                    case MarkerType::TOCODASYM:
                    case MarkerType::DA_CODA:
                    case MarkerType::DA_DBLCODA: direction_marker(xml_, mk, jump_elements_); break;
                    case MarkerType::SEGNO:
                    case MarkerType::VARSEGNO:
                    case MarkerType::CODA:
                    case MarkerType::VARCODA:
                    case MarkerType::CODETTA:
                        // ignore
                        break;
                    case MarkerType::USER: LOGD("repeatAtMeasureStop: marker %d not implemented", int(mtp)); break;
                }
            } break;
            case ElementType::JUMP: direction_jump(xml_, to_jump(e)); break;
            default: LOGD("repeatAtMeasureStop: direction type %s at tick %d not implemented", e->type_name(), m->tick().ticks()); break;
        }
    }
}

//---------------------------------------------------------
//  work -- write the <work> element
//  note that order must be work-number, work-title
//  also write <movement-number> and <movement-title>
//  data is taken from the score metadata instead of the Text elements
//---------------------------------------------------------

void ExportMusicXml::work(const MeasureBase * /*measure*/) {
    const String work_title = score_->meta_tag(u"workTitle");
    const String work_number = score_->meta_tag(u"workNumber");
    if (!(work_title.is_empty() && work_number.is_empty())) {
        xml_.start_element("work");
        if (!work_number.is_empty()) {
            xml_.tag("work-number", work_number);
        }
        if (!work_title.is_empty()) {
            xml_.tag("work-title", work_title);
        }
        xml_.end_element();
    }
    if (!score_->meta_tag(u"movementNumber").is_empty()) {
        xml_.tag("movement-number", score_->meta_tag(u"movementNumber"));
    }
    if (!score_->meta_tag(u"movementTitle").is_empty()) {
        xml_.tag("movement-title", score_->meta_tag(u"movementTitle"));
    }
}

//---------------------------------------------------------
//  measureRepeat -- write measure-repeat
//---------------------------------------------------------

static void measure_repeat(XmlWriter &xml, Attributes &attr, const Measure *const m, const int part_index) {
    Part *part = m->score()->parts().at(part_index);
    const staff_idx_t score_rel_staff = m->score()->staff_idx(part);
    for (size_t i = 0; i < part->nstaves(); ++i) {
        staff_idx_t staff_idx = score_rel_staff + i;
        XmlWriter::Attributes style_attrs;
        if (part->nstaves() > 1) {
            style_attrs.emplace_back(std::make_pair("number", i + 1));
        }
        if (m->is_measure_repeat_group(staff_idx) &&
            (!m->prev_measure() || !m->prev_measure()->is_measure_repeat_group(staff_idx) ||
             (m->measure_repeat_num_measures(staff_idx) != m->prev_measure()->measure_repeat_num_measures(staff_idx)))) {
            attr.do_attr(xml, true);
            add_color_attr(m->measure_repeat_element(staff_idx), style_attrs);
            xml.start_element("measure-style", style_attrs);
            const int num_measures = m->measure_repeat_num_measures(staff_idx);
            if (num_measures > 1) {
                // slashes == numMeasures for everything MuseScore currently supports
                xml.tag("measure-repeat", {{"slashes", num_measures}, {"type", "start"}}, num_measures);
            } else {
                // no need to include slashes
                xml.tag("measure-repeat", {{"type", "start"}}, num_measures);
            }
            xml.end_element();
        } else if (
            // no longer in measure repeats
            m->prev_measure() &&
            ((m->prev_measure()->is_measure_repeat_group(staff_idx) && !m->is_measure_repeat_group(staff_idx))
             // or still in measure repeats, but now of different duration
             || (m->prev_measure()->measure_repeat_element(staff_idx) && m->measure_repeat_element(staff_idx) &&
                 (m->measure_repeat_num_measures(staff_idx) != m->prev_measure()->measure_repeat_num_measures(staff_idx))))) {
            attr.do_attr(xml, true);
            xml.start_element("measure-style", style_attrs);
            xml.tag("measure-repeat", {{"type", "stop"}}, "");
            xml.end_element();
        }
    }
}

//---------------------------------------------------------
//  measureStyle -- write measure-style
//---------------------------------------------------------

// this is done at the first measure of a multimeasure rest
// note: for a normal measure, mmRest1 is the measure itself,
// for a multi-measure rest, it is the replacing measure

static void measure_style(XmlWriter &xml, Attributes &attr, const Measure *const m, const int part_index) {
    const Measure *mm_r1 = m->covering_mm_rest_or_this();
    if (m != mm_r1 && m == mm_r1->mm_rest_first()) {
        attr.do_attr(xml, true);
        xml.start_element("measure-style");
        String multi_rest_tag = u"multiple-rest";
        if (m->score()->style().style_b(Sid::OLD_STYLE_MULTI_MEASURE_RESTS)) {
            if (mm_r1->mm_rest_count() <= m->score()->style().style_i(Sid::MM_REST_OLD_STYLE_MAX_MEASURES)) {
                multi_rest_tag += u" use-symbols=\"yes\"";
            } else {
                multi_rest_tag += u" use-symbols=\"no\"";
            }
        }
        xml.tag_raw(multi_rest_tag, mm_r1->mm_rest_count());
        xml.end_element();
    } else {
        // measure repeat can only possibly be present if mmrest was not
        measure_repeat(xml, attr, m, part_index);
    }
}

//---------------------------------------------------------
//  commonAnnotations
//---------------------------------------------------------

static bool common_annotations(ExportMusicXml *exp, const EngravingItem *e, staff_idx_t sstaff) {
    if (!exp->can_write(e)) {
        // write only tempo and swing
        if (e->is_tempo_text()) {
            exp->tempo_sound(to_tempo_text(e));
        } else if (e->is_system_text()) {
            const StaffTextBase *text = to_staff_text_base(e);
            if (text->swing()) {
                exp->swing_sound(text, true);
            }
        }
        return false;
    }

    // note: the instrument change details are handled in ExportMusicXml::writeMeasureTracks,
    // optionally writing the associated staff text is done below
    if (e->is_tempo_text()) {
        exp->tempo_text(to_tempo_text(e), sstaff);
    } else if (e->is_play_tech_annotation()) {
        exp->play_text(to_play_tech_annotation(e), sstaff);
    } else if (e->is_capo() || e->is_string_tunings() || e->is_staff_text() || e->is_triplet_feel() || e->is_text() || e->is_expression() ||
               (e->is_instrument_change() && e->visible()) || e->is_sticking()) {
        exp->words(to_text_base(e), sstaff);
    } else if (e->is_dynamic()) {
        exp->dynamic(to_dynamic(e), sstaff);
    } else if (e->is_harp_pedal_diagram()) {
        exp->harp_pedals(to_harp_pedal_diagram(e), sstaff);
    } else if (e->is_rehearsal_mark()) {
        exp->rehearsal(to_rehearsal_mark(e), sstaff);
    } else if (e->is_image()) {
        exp->image(to_image(e), sstaff);
    } else if (e->is_system_text()) {
        exp->system_text(to_staff_text_base(e), sstaff);
    }

    return true;
}

//---------------------------------------------------------
//  annotations
//---------------------------------------------------------

// Only handle common annotations, others are handled elsewhere

static void annotations(ExportMusicXml *exp, track_idx_t strack, track_idx_t etrack, track_idx_t track, staff_idx_t sstaff, Segment *seg) {
    for (const EngravingItem *e : seg->annotations()) {
        track_idx_t wtrack = muse::nidx; // track to write annotation

        if (strack <= e->track() && e->track() < etrack) {
            wtrack = find_track_for_annotations(e->track(), seg);
        }

        if (track == wtrack) {
            if (common_annotations(exp, e, sstaff)) {
                // already handled
            }
        }
    }
}

//---------------------------------------------------------
//  harmonies
//---------------------------------------------------------

/*
 * Helper method to export harmonies and chord diagrams for a single segment.
 */

static void segment_harmonies(ExportMusicXml *exp, track_idx_t track, Segment *seg, Fraction offset) {
    const std::vector<EngravingItem *> diagrams = seg->find_annotations(ElementType::FRET_DIAGRAM, track, track);
    std::vector<EngravingItem *> harmonies = seg->find_annotations(ElementType::HARMONY, track, track);

    for (const EngravingItem *d : diagrams) {
        const FretDiagram *diagram = to_fret_diagram(d);
        const Harmony *harmony = diagram->harmony();
        if (harmony) {
            exp->harmony(harmony, diagram, offset);
        } else if (!harmonies.empty()) {
            const EngravingItem *default_harmony = harmonies.back();
            exp->harmony(to_harmony(default_harmony), diagram, offset);
            harmonies.pop_back();
        } else {
            // Found a fret diagram with no harmony, ignore
            LOGD("segmentHarmonies() seg %p found fretboard diagram %p w/o harmony: cannot write", seg, diagram);
        }
    }

    for (const EngravingItem *h : harmonies) {
        exp->harmony(to_harmony(h), 0, offset);
    }
}

/*
 * Write harmonies and fret diagrams that are attached to chords or rests.
 *
 * There are fondamental differences between the ways Musescore and MusicXML handle harmonies (Chord symbols)
 * and fretboard diagrams.
 *
 * In MuseScore, the Harmony element is now a child of FretboardDiagram BUT in previous versions,
 * both elements were independent siblings so we have to handle both cases.
 * In MusicXML, fretboard diagram is always contained in a harmony element.
 *
 * In MuseScore, Harmony elements are not always linked to notes, and each Harmony will be contained
 * in a `ChordRest` Segment.
 * In MusicXML, those successive Harmony elements must be exported before the note with different offsets.
 *
 * Edge cases that we simply cannot handle:
 *  - as of MusicXML 3.1, there is no way to represent a diagram without an associated chord symbol,
 * so when we encounter such an object in MuseScore, we simply cannot export it.
 *  - If a ChordRest segment contans a FretboardDiagram with no harmonies and several different Harmony siblings,
 * we simply have to pick a random one to export.
 */

static void harmonies(ExportMusicXml *exp, track_idx_t track, Segment *seg) {
    Fraction offset = {0, 1};
    segment_harmonies(exp, track, seg, offset);

    // Edge case: find remaining `harmony` elements.
    // Suppose you have one single whole note in the measure but several chord symbols.
    // In MuseScore, each `Harmony` object will be stored in a `ChordRest` Segment that contains
    // no other Chords.
    // But in MusicXML, you are supposed to output all `harmony` elements before the first `note`,
    // with different `offset` parameters.
    //
    // That's why we need to explore the remaining segments to find
    // `Harmony` and `FretDiagram` elements in Segments without Chords and output them now.
    for (Segment *seg1 = seg->next(); seg1; seg1 = seg1->next()) {
        if (!seg1->is_chord_rest_type()) {
            continue;
        }

        const EngravingItem *el1 = seg1->element(track);
        if (el1) { // found a ChordRest, next harmony will be attached to this one
            break;
        }
        offset = (seg1->tick() - seg->tick());
        segment_harmonies(exp, track, seg1, offset);
    }
}

//---------------------------------------------------------
//   Write MusicXML
//
// Writes the portion within the <figure> tag.
//
// NOTE: Both MuseScore and MusicXML provide two ways of altering the (temporal) length of a
// figured bass object: extension lines and duration. The convention is that an EXTENSION is
// used if the figure lasts LONGER than the note (i.e., it "extends" to the following notes),
// whereas DURATION is used if the figure lasts SHORTER than the note (e.g., when notating a
// figure change under a note). However, MuseScore does not restrict durations in this way,
// allowing them to act as extensions themselves. As a result, a few more branches are
// required in the decision tree to handle everything correctly.
//---------------------------------------------------------

//---------------------------------------------------------
//   Convert Modifier to MusicXML prefix/suffix
//---------------------------------------------------------

static String modifier2_music_xml(FiguredBassItem::Modifier prefix) {
    switch (prefix) {
        case FiguredBassItem::Modifier::NONE: return String();
        case FiguredBassItem::Modifier::DOUBLEFLAT: return u"flat-flat";
        case FiguredBassItem::Modifier::FLAT: return u"flat";
        case FiguredBassItem::Modifier::NATURAL: return u"natural";
        case FiguredBassItem::Modifier::SHARP: return u"sharp";
        case FiguredBassItem::Modifier::DOUBLESHARP: return u"double-sharp";
        case FiguredBassItem::Modifier::CROSS: return u"cross";
        case FiguredBassItem::Modifier::BACKSLASH: return u"backslash";
        case FiguredBassItem::Modifier::SLASH: return u"slash";
        case FiguredBassItem::Modifier::NUMOF: return String(); // prevent gcc "‘FBINumOfAccid’ not handled in switch" warning
    }
    return String();
}

static void write_music_xml(const FiguredBassItem *item, XmlWriter &xml, bool is_original_figure, int cr_end_tick, int fb_end_tick) {
    xml.start_element("figure");

    // The first figure of each group is the "original" figure. Practically, it is one inserted manually
    // by the user, rather than automatically by the "duration" extend method.
    if (is_original_figure) {
        String str_prefix = modifier2_music_xml(item->prefix());
        if (!str_prefix.empty()) {
            xml.tag("prefix", str_prefix);
        }
        if (item->digit() != ::mu::engraving::fbi_digit_none) {
            xml.tag("figure-number", item->digit());
        }
        String str_suffix = modifier2_music_xml(item->suffix());
        if (!str_suffix.empty()) {
            xml.tag("suffix", str_suffix);
        }

        // Check if the figure ends before or at the same time as the current note. Otherwise, the figure
        // extends to the next note, and so carries an extension type "start" by definition.
        if (fb_end_tick <= cr_end_tick) {
            if (item->cont_line() == FiguredBassItem::ContLine::SIMPLE) {
                xml.tag("extend", {{"type", "stop"}});
            } else if (item->cont_line() == FiguredBassItem::ContLine::EXTENDED) {
                bool has_figure = (!str_prefix.empty() || item->digit() != ::mu::engraving::fbi_digit_none || !str_suffix.empty());
                if (has_figure) {
                    xml.tag("extend", {{"type", "start"}});
                } else {
                    xml.tag("extend", {{"type", "continue"}});
                }
            }
        } else {
            xml.tag("extend", {{"type", "start"}});
        }
    }
    // If the figure is not "original", it must have been created using the "duration" feature of figured bass.
    // In other words, the original figure belongs to a previous note rather than the current note.
    else {
        if (cr_end_tick < fb_end_tick) {
            xml.tag("extend", {{"type", "continue"}});
        } else {
            xml.tag("extend", {{"type", "stop"}});
        }
    }
    xml.end_element();
}

static void write_music_xml(const FiguredBass *item, XmlWriter &xml, bool is_original_figure, int cr_end_tick, int fb_end_tick,
                            bool write_duration, int divisions) {
    XmlWriter::Attributes attrs;
    if (item->parentheses_mode()) {
        attrs = {{"parentheses", "yes"}};
    }
    if (item->place_above()) {
        attrs.emplace_back(std::make_pair("placement", "above"));
    }
    add_color_attr(item, attrs);
    if (!item->visible()) {
        attrs.emplace_back(std::make_pair("print-object", "no"));
    }
    xml.start_element("figured-bass", attrs);
    for (FiguredBassItem *fb_item : item->items()) {
        write_music_xml(fb_item, xml, is_original_figure, cr_end_tick, fb_end_tick);
    }
    if (write_duration) {
        xml.tag("duration", calculate_duration_in_divisions(item->ticks(), divisions));
    }
    xml.end_element();
}

//---------------------------------------------------------
//  figuredBass
//---------------------------------------------------------

static void figured_bass(XmlWriter &xml, track_idx_t strack, track_idx_t etrack, track_idx_t track, const ChordRest *cr, FigBassMap &fb_map,
                         int divisions) {
    Segment *seg = cr->segment();
    if (seg->segment_type() == SegmentType::CHORD_REST) {
        for (const EngravingItem *e : seg->annotations()) {
            track_idx_t wtrack = muse::nidx; // track to write annotation

            if (strack <= e->track() && e->track() < etrack) {
                wtrack = find_track_for_annotations(e->track(), seg);
            }

            if (track == wtrack) {
                if (e->is_figured_bass()) {
                    const FiguredBass *fb = to_figured_bass(e);
                    if (fb->items().empty()) {
                        continue;
                    }
                    //LOGD("figuredbass() track %d seg %p fb %p seg %p tick %d ticks %d cr %p tick %d ticks %d",
                    //       track, seg, fb, fb->segment(), fb->segment()->tick(), fb->ticks(), cr, cr->tick(), cr->actualTicks());
                    bool extend = fb->ticks() > cr->actual_ticks();
                    if (extend) {
                        //LOGD("figuredbass() extend to %d + %d = %d",
                        //       cr->tick(), fb->ticks(), cr->tick() + fb->ticks());
                        fb_map.insert({strack, fb});
                    } else {
                        muse::remove(fb_map, strack);
                    }
                    const Fraction cr_end_tick = cr->end_tick();
                    const Fraction fb_end_tick = fb->segment()->tick() + fb->ticks();
                    const bool write_duration = fb->ticks() < cr->actual_ticks();
                    write_music_xml(fb, xml, true, cr_end_tick.ticks(), fb_end_tick.ticks(), write_duration, divisions);
                    // Check for changing figures under a single note (each figure stored in a separate segment)
                    for (Segment *seg_next = seg->next(); seg_next && seg_next->element(track) == nullptr; seg_next = seg_next->next()) {
                        for (EngravingItem *annot : seg_next->annotations()) {
                            if (annot->is_figured_bass() && annot->track() == track) {
                                fb = to_figured_bass(annot);
                                write_music_xml(fb, xml, true, 0, 0, true, divisions);
                            }
                        }
                    }
                    // no extend can be pending
                    return;
                }
            }
        }
        // check for extend pending
        if (muse::contains(fb_map, strack)) {
            const FiguredBass *fb = fb_map.at(strack);
            Fraction cr_end_tick = cr->end_tick();
            Fraction fb_end_tick = fb->segment()->tick() + fb->ticks();
            bool write_duration = fb->ticks() < cr->actual_ticks();
            if (cr->tick() < fb_end_tick) {
                //LOGD("figuredbass() at tick %d extend only", cr->tick());
                write_music_xml(fb, xml, false, cr_end_tick.ticks(), fb_end_tick.ticks(), write_duration, divisions);
            }
            if (fb_end_tick <= cr_end_tick) {
                //LOGD("figuredbass() at tick %d extend done", cr->endTick());
                muse::remove(fb_map, strack);
            }
        }
    }
}

//---------------------------------------------------------
//  spannerStart
//---------------------------------------------------------

// for each spanner start:
// find start track
// find stop track
// if stop track < start track
//   get data from list of already stopped spanners
// else
//   calculate data
// write start if in right track

static void spanner_start(ExportMusicXml *exp, track_idx_t strack, track_idx_t etrack, track_idx_t track, staff_idx_t sstaff,
                          Segment *seg) {
    if (seg->is_chord_rest_type() || seg->is_time_tick_type()) {
        Fraction stick = seg->tick();
        for (auto it = exp->score()->spanner().lower_bound(stick.ticks()); it != exp->score()->spanner().upper_bound(stick.ticks()); ++it) {
            Spanner *e = it->second;

            if (!exp->can_write(e)) {
                continue;
            }

            track_idx_t wtrack = muse::nidx; // track to write spanner
            if (strack <= e->track() && e->track() < etrack) {
                wtrack = find_track_for_annotations(e->track(), seg);
            }

            if (track == wtrack) {
                switch (e->type()) {
                    case ElementType::HAIRPIN: exp->hairpin(to_hairpin(e), sstaff, seg->tick()); break;
                    case ElementType::OTTAVA: exp->ottava(to_ottava(e), sstaff, seg->tick()); break;
                    case ElementType::PEDAL: exp->pedal(to_pedal(e), sstaff, seg->tick()); break;
                    case ElementType::TEXTLINE: exp->text_line(to_text_line_base(e), sstaff, seg->tick()); break;
                    case ElementType::LET_RING: exp->text_line(to_let_ring(e), sstaff, seg->tick()); break;
                    case ElementType::GRADUAL_TEMPO_CHANGE: exp->text_line(to_gradual_tempo_change(e), sstaff, seg->tick()); break;
                    case ElementType::PALM_MUTE: exp->text_line(to_palm_mute(e), sstaff, seg->tick()); break;
                    case ElementType::WHAMMY_BAR: exp->text_line(to_whammy_bar(e), sstaff, seg->tick()); break;
                    case ElementType::RASGUEADO: exp->text_line(to_rasgueado(e), sstaff, seg->tick()); break;
                    case ElementType::HARMONIC_MARK: exp->text_line(to_harmonic_mark(e), sstaff, seg->tick()); break;
                    case ElementType::PICK_SCRAPE: exp->text_line(to_pick_scrape(e), sstaff, seg->tick()); break;
                    case ElementType::TRILL:
                        // ignore (written as <note><notations><ornaments><wavy-line>)
                        break;
                    case ElementType::SLUR:
                        // ignore (written as <note><notations><slur>)
                        break;
                    default:
                        LOGD("spannerStart: direction type %d ('%s') at tick %d not implemented", int(e->type()), e->type_name(),
                             seg->tick().ticks());
                        break;
                }
            }
        } // for
    }
}

//---------------------------------------------------------
//  spannerStop
//---------------------------------------------------------

// called after writing each chord or rest to check if a spanner must be stopped
// loop over all spanners and find spanners in strack ending at tick2
// note that more than one voice may contains notes ending at tick2,
// remember which spanners have already been stopped (the "stopped" set)

static void spanner_stop(ExportMusicXml *exp, track_idx_t strack, track_idx_t etrack, const Fraction &tick2, staff_idx_t sstaff,
                         std::set<const Spanner *> &stopped) {
    for (auto it : exp->score()->spanner()) {
        const Spanner *e = it.second;

        if (!exp->can_write(e)) {
            continue;
        }

        if (e->tick2() != tick2 || e->track() < strack || e->track() >= etrack) {
            continue;
        }

        if (!muse::contains(stopped, e)) {
            stopped.insert(e);
            switch (e->type()) {
                case ElementType::HAIRPIN: exp->hairpin(to_hairpin(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::OTTAVA: exp->ottava(to_ottava(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::PEDAL: exp->pedal(to_pedal(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::TEXTLINE: exp->text_line(item_cast<const TextLineBase *>(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::LET_RING: exp->text_line(to_let_ring(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::GRADUAL_TEMPO_CHANGE: exp->text_line(to_gradual_tempo_change(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::PALM_MUTE: exp->text_line(to_palm_mute(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::WHAMMY_BAR: exp->text_line(to_whammy_bar(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::RASGUEADO: exp->text_line(to_rasgueado(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::HARMONIC_MARK: exp->text_line(to_harmonic_mark(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::PICK_SCRAPE: exp->text_line(to_pick_scrape(e), sstaff, Fraction(-1, 1)); break;
                case ElementType::TRILL:
                    // ignore (written as <note><notations><ornaments><wavy-line>
                    break;
                case ElementType::SLUR:
                    // ignore (written as <note><notations><slur>)
                    break;
                default: LOGD("spannerStop: direction type %s at tick2 %d not implemented", e->type_name(), tick2.ticks()); break;
            }
        }
    } // for
}

//---------------------------------------------------------
//  keysigTimesig
//---------------------------------------------------------

/**
 Output attributes at start of measure: key, time
 */

void ExportMusicXml::keysig_timesig(const Measure *m, const Part *p) {
    const TrackRange track_range = p->track_range();

    // search all staves for non-generated time and key signatures
    std::map<staff_idx_t, KeySig *> keysigs;   // map staff to key signature
    std::map<staff_idx_t, TimeSig *> timesigs; // map staff to time signature
    for (Segment *seg = m->first(); seg; seg = seg->next()) {
        if (seg->tick() > m->tick()) {
            break;
        }
        for (track_idx_t t = track_range.start_track; t < track_range.end_track; t += ::mu::engraving::voices) {
            EngravingItem *el = seg->element(t);
            if (!el) {
                continue;
            }
            if (el->is_key_sig()) {
                //LOGD(" found keysig %p track %d", el, el->track());
                staff_idx_t st = (t - track_range.start_track) / ::mu::engraving::voices;
                if (!el->generated()) {
                    keysigs[st] = to_key_sig(el);
                }
            }

            if (el->is_time_sig()) {
                LOGN(" found timesig %p tick %d track %zu", el, el->tick().ticks(), el->track());
                staff_idx_t st = (t - track_range.start_track) / ::mu::engraving::voices;
                if (!el->generated()) {
                    timesigs[st] = to_time_sig(el);
                }
            }
        }
    }

    // write the key signatues
    if (!keysigs.empty()) {
        // determine if all staves have a keysig and all keysigs are identical
        // in that case a single <key> is written, without number=... attribute
        size_t nstaves = p->nstaves();
        bool single_key = true;
        // check if all staves have a keysig
        for (staff_idx_t i = 0; i < nstaves; i++) {
            if (!muse::contains(keysigs, i)) {
                single_key = false;
            }
        }
        // check if all keysigs are identical
        if (single_key) {
            for (staff_idx_t i = 1; i < nstaves; i++) {
                if (!(keysigs.at(i)->key() == keysigs.at(0)->key())) {
                    single_key = false;
                }
            }
        }
        if (single_key) {
            auto key_print_object = [](const KeySig *ks) {
                return !ks->visible() || (ks->staff() && ks->staff()->is_tab_staff(ks->tick()));
            };
            for (staff_idx_t i = 1; i < nstaves; i++) {
                if (key_print_object(keysigs.at(i)) != key_print_object(keysigs.at(0))) {
                    single_key = false;
                    break;
                }
            }
        }

        // write the keysigs
        //LOGD(" singleKey %d", singleKey);
        if (single_key) {
            // keysig applies to all staves
            keysig(keysigs.at(0), p->staff(0)->clef(m->tick()), 0);
        } else {
            // staff-specific keysigs
            for (const auto &[st, ks] : keysigs) {
                keysig(ks, p->staff(st)->clef(m->tick()), st + 1);
            }
        }
    } else {
        // always write a keysig at tick = 0
        if (m->tick().is_zero()) {
            //KeySigEvent kse;
            //kse.setKey(Key::C);
            KeySig *ks = Factory::create_key_sig(score_->dummy());
            if (p->staff(0)->is_tab_staff(Fraction(0, 1))) {
                ks->set_visible(false);
            }
            ks->set_key(Key::C);
            keysig(ks, p->staff(0)->clef(m->tick()));
            delete ks;
        }
    }

    // write the time signatues
    if (!timesigs.empty()) {
        // determine if all staves have a timesig and all timesigs are identical
        // in that case a single <time> is written, without number=... attribute
        size_t nstaves = p->nstaves();
        bool single_time = true;
        // check if all staves have a keysig
        for (staff_idx_t i = 0; i < nstaves; i++) {
            if (!muse::contains(timesigs, i)) {
                single_time = false;
            }
        }
        // check if all timesigs are identical
        if (single_time) {
            for (staff_idx_t i = 1; i < nstaves; i++) {
                if (!(timesigs.at(i)->sig() == timesigs.at(0)->sig())) {
                    single_time = false;
                    break;
                }
            }
        }
        if (single_time) {
            auto time_print_object = [](const TimeSig *ts) {
                return !ts->visible() || !ts->show_on_this_staff() || (ts->staff() && !ts->staff()->staff_type(ts->tick())->gen_timesig());
            };
            for (staff_idx_t i = 1; i < nstaves; i++) {
                if (time_print_object(timesigs.at(i)) != time_print_object(timesigs.at(0))) {
                    single_time = false;
                    break;
                }
            }
        }

        // write the timesigs
        LOGN(" singleTime %d", single_time);
        if (single_time) {
            // timesig applies to all staves
            timesig(timesigs.at(0), 0);
        } else {
            // staff-specific timesig
            for (const auto &[st, ts] : timesigs) {
                timesig(ts, st + 1);
            }
        }
    }
}

//---------------------------------------------------------
//  identification -- write the identification
//---------------------------------------------------------

void ExportMusicXml::identification(XmlWriter &xml, Score const *const score) {
    xml.start_element("identification");

    // the creator types commonly found in MusicXML
    std::set<String> meta_tag_names = {u"arranger", u"composer", u"lyricist", u"poet", u"translator"};
    for (const String &type : meta_tag_names) {
        String creator = score->meta_tag(type);
        if (!creator.is_empty()) {
            xml.tag("creator", {{"type", type}}, creator);
        }
    }

    if (!score->meta_tag(u"copyright").is_empty()) {
        xml.tag("rights", score->meta_tag(u"copyright"));
        meta_tag_names.emplace(u"copyright");
    }

    xml.start_element("encoding");

    String encoder = score->meta_tag(u"encoder");
    if (!encoder.empty()) {
        xml.tag("encoder", encoder);
    }

    if (MScore::debug_mode) {
        xml.tag("software", String(u"MuseScore 0.7.0"));
        xml.tag("encoding-date", String(u"2007-09-10"));
    } else {
        xml.tag("software", String(u"MuseScore Studio ") + muse::appinfo::version().to_string());
        xml.tag("encoding-date", muse::Date::current_date().to_string(muse::DateFormat::ISO_DATE));
    }

    // specify supported elements
    xml.tag("supports", {{"element", "accidental"}, {"type", "yes"}});
    xml.tag("supports", {{"element", "beam"}, {"type", "yes"}});
    // set support for print new-page and new-system to match user preference
    // for MusicXmlExportBreaks::MANUAL support is "no" because "yes" breaks Finale NotePad import
    IMusicXmlConfiguration::MusicXmlExportBreaksType breaks_type = configuration()->export_breaks_type();
    if (configuration()->export_layout() && breaks_type == IMusicXmlConfiguration::MusicXmlExportBreaksType::ALL) {
        xml.tag("supports", {{"element", "print"}, {"attribute", "new-page"}, {"type", "yes"}, {"value", "yes"}});
        xml.tag("supports", {{"element", "print"}, {"attribute", "new-system"}, {"type", "yes"}, {"value", "yes"}});
    } else {
        xml.tag("supports", {{"element", "print"}, {"attribute", "new-page"}, {"type", "no"}});
        xml.tag("supports", {{"element", "print"}, {"attribute", "new-system"}, {"type", "no"}});
    }
    xml.tag("supports", {{"element", "stem"}, {"type", "yes"}});

    xml.end_element();

    if (!score->meta_tag(u"source").is_empty()) {
        xml.tag("source", score->meta_tag(u"source"));
        meta_tag_names.emplace(u"source");
    }

    if (!MScore::debug_mode) {
        // do not write miscellaneous in debug mode
        meta_tag_names.insert({u"workTitle", u"workNumber", u"movementTitle", u"movementNumber", u"originalFormat"});
        xml.start_element("miscellaneous");
        for (const auto &meta_tag : score->meta_tags()) {
            auto search = meta_tag_names.find(meta_tag.first);
            if (search != meta_tag_names.end()) {
                continue;
            } else if (!meta_tag.second.is_empty()) {
                xml.tag("miscellaneous-field", {{"name", meta_tag.first}}, meta_tag.second);
            }
        }
        xml.end_element();
    }

    xml.end_element();
}

//---------------------------------------------------------
//  findPartGroupNumber
//---------------------------------------------------------

static int find_part_group_number(std::array<int, ::mu::iex::musicxml::max_part_groups> part_group_end) {
    // find part group number
    for (int number = 0; number < ::mu::iex::musicxml::max_part_groups; ++number) {
        if (part_group_end[number] == -1) {
            return number;
        }
    }
    LOGD("no free part group number");
    return ::mu::iex::musicxml::max_part_groups;
}

//---------------------------------------------------------
//  scoreInstrument
//---------------------------------------------------------

static void score_instrument(XmlWriter &xml, const size_t part_nr, const int instr_nr, const String &instr_name,
                             const Instrument *instr = nullptr) {
    xml.start_element_raw(String(u"score-instrument %1").arg(instr_id(part_nr, instr_nr)));
    xml.tag("instrument-name", instr_name);
    if (instr && !instr->music_xml_id().is_empty() && !MScore::test_mode) {
        xml.tag("instrument-sound", instr->music_xml_id());
    }
    xml.end_element();
}

//---------------------------------------------------------
//  midiInstrument
//---------------------------------------------------------

static void midi_instrument(XmlWriter &xml, const size_t part_nr, const int instr_nr, const Instrument *instr, const Score *score,
                            const int unpitched = 0) {
    xml.start_element_raw(String(u"midi-instrument %1").arg(instr_id(part_nr, instr_nr)));
    int midi_channel = score->master_score()->midi_channel(instr->channel(0)->channel());
    if (midi_channel >= 0 && midi_channel < 16) {
        xml.tag("midi-channel", midi_channel + 1);
    }
    int midi_program = instr->channel(0)->program();
    if (midi_program >= 0 && midi_program < 128) {
        xml.tag("midi-program", midi_program + 1);
    }
    if (unpitched > 0) {
        xml.tag("midi-unpitched", unpitched);
    }
    xml.tag("volume", (instr->channel(0)->volume() / 127.0) * 100);       //percent
    xml.tag("pan", int(((instr->channel(0)->pan() - 63.5) / 63.5) * 90)); //-90 hard left, +90 hard right      xml.etag();
    xml.end_element();
}

//---------------------------------------------------------
//  initInstrMap
//---------------------------------------------------------

/**
 Initialize the Instrument* to number map for a Part
 Used to generate instrument numbers for a multi-instrument part
 */

static void init_instr_map(MusicXmlInstrumentMap &im, const InstrumentList &il, const Score * /*score*/) {
    im.clear();
    for (const auto &pair : il) {
        const Instrument *instr = pair.second;
        if (!muse::contains(im, instr)) {
            im.insert({instr, static_cast<int>(im.size())});
        }
    }
}

//---------------------------------------------------------
//  initReverseInstrMap
//---------------------------------------------------------

typedef std::map<int, const Instrument *> MusicXmlReverseInstrumentMap;

/**
 Initialize the number t Instrument* map for a Part
 Used to iterate in sequence over instrument numbers for a multi-instrument part
 */

static void init_reverse_instr_map(MusicXmlReverseInstrumentMap &rim, const MusicXmlInstrumentMap &im) {
    rim.clear();

    for (const auto &[instr, inst_nr] : im) {
        rim.insert({inst_nr, instr});
    }
}

//---------------------------------------------------------
//  hasPageBreak
//---------------------------------------------------------

static MeasureBase *last_measure_base(const System *const system) {
    MeasureBase *mb = nullptr;
    if (system) {
        const auto &measures = system->measures();
        IF_ASSERT_FAILED(!(measures.empty())) { return nullptr; }
        mb = measures.back();
    }
    return mb;
}

//---------------------------------------------------------
//  hasPageBreak
//---------------------------------------------------------

static bool has_page_break(const System *const system) {
    const MeasureBase *mb = nullptr;
    if (system) {
        const auto &measures = system->measures();
        IF_ASSERT_FAILED(!(measures.empty())) { return false; }
        mb = measures.back();
    }

    return mb && mb->page_break();
}

//---------------------------------------------------------
//  print
//---------------------------------------------------------

/**
 Handle the <print> element.
 When exporting layout and all breaks, a <print> with layout information
 is generated for the first measure in the score, in a system or on a page.
 When exporting layout but only manual or no breaks, a <print> with
 layout information is generated only for the first measure in the score,
 as it is assumed the system layout is broken by the importing application
 anyway and is thus useless.

 a page break is explicit (manual) if:
 - the last system on the previous page has a page break
 a system break is explicit (manual) if:
 - the previous system in the score has a system or layout break
 - if the previous system in the score does not have measures
   (i.e. only has (a) frame(s))
 */

void ExportMusicXml::print(const Measure *const m, const int part_nr, const int first_staff_of_part, const size_t nr_staves_in_part,
                           const MeasurePrintContext &mpc) {
    const MeasureBase *prev_sys_mb = last_measure_base(mpc.prev_system);

    if (prev_sys_mb && prev_sys_mb->is_measure()) {
        const Measure *mm_rest = to_measure(prev_sys_mb)->covering_mm_rest_or_this();
        if (mm_rest->is_mm_rest() && mm_rest->mm_rest_last() == prev_sys_mb) {
            prev_sys_mb = mm_rest;
        }
    }

    const bool prev_meas_line_break = prev_sys_mb ? prev_sys_mb->line_break() : false;
    const bool prev_meas_section_break = prev_sys_mb ? prev_sys_mb->section_break() : false;
    const bool prev_page_break = has_page_break(mpc.last_system_prev_page);

    XmlWriter::Attributes attributes;

    IMusicXmlConfiguration::MusicXmlExportBreaksType export_breaks_type = configuration()->export_breaks_type();
    if (!mpc.score_start) {
        if (export_breaks_type == IMusicXmlConfiguration::MusicXmlExportBreaksType::ALL) {
            if (mpc.page_start) {
                attributes.push_back({"new-page", "yes"});
            } else if (mpc.system_start) {
                attributes.push_back({"new-system", "yes"});
            }
        } else if (export_breaks_type == IMusicXmlConfiguration::MusicXmlExportBreaksType::MANUAL) {
            if (mpc.page_start && prev_page_break) {
                attributes.push_back({"new-page", "yes"});
            } else if (mpc.system_start && (prev_meas_line_break || prev_meas_section_break)) {
                attributes.push_back({"new-system", "yes"});
            }
        }
    }

    bool do_break = mpc.score_start || !attributes.empty();
    bool do_layout = configuration()->export_layout();

    if (mpc.page_start) {
        bool export_page_no =
            export_breaks_type == IMusicXmlConfiguration::MusicXmlExportBreaksType::ALL ||
            (export_breaks_type == IMusicXmlConfiguration::MusicXmlExportBreaksType::MANUAL && mpc.page_start && prev_page_break);
        if (export_page_no && mpc.write_page_no) {
            attributes.push_back({"page-number", mpc.page_number});
        }
    }

    if (do_break && do_layout) {
        xml_.start_element("print", attributes);
        const MStyle &style = score()->style();
        const double page_width = get_tenths_from_inches(style.style_d(Sid::PAGE_WIDTH));
        const double lm = get_tenths_from_inches(style.style_d(Sid::PAGE_ODD_LEFT_MARGIN));
        const double rm = get_tenths_from_inches(style.style_d(Sid::PAGE_WIDTH) - style.style_d(Sid::PAGE_PRINTABLE_WIDTH) -
                                                 style.style_d(Sid::PAGE_ODD_LEFT_MARGIN));
        const double tm = get_tenths_from_inches(style.style_d(Sid::PAGE_ODD_TOP_MARGIN));

        // System Layout

        // For a multi-measure rest positioning is valid only
        // in the replacing measure
        // note: for a normal measure, mmRest1 is the measure itself,
        // for a multi-measure rest, it is the replacing measure
        const Measure *mm_r1 = m->covering_mm_rest_or_this();
        const System *system = mm_r1->system();

        // Put the system print suggestions only for the first part in a score...
        if (part_nr == 0) {
            // Find the right margin of the system.
            double system_lm = get_tenths_from_dots(mm_r1->page_pos().x() - system->page()->page_pos().x()) - lm;
            double system_rm = page_width - rm - (get_tenths_from_dots(system->ldata()->bbox().width()) + lm);

            xml_.start_element("system-layout");
            xml_.start_element("system-margins");
            xml_.tag("left-margin", String::number(system_lm, 2));
            xml_.tag("right-margin", String::number(system_rm, 2));
            xml_.end_element();

            if (mpc.system_start && !mpc.page_start) {
                // see System::layout2() for the factor 2 * score()->spatium()
                const Measure *prev_system_measure = mpc.prev_measure->covering_mm_rest_or_this();
                const double sys_dist =
                    get_tenths_from_dots(mm_r1->page_pos().y() - prev_system_measure->page_pos().y() -
                                         prev_system_measure->ldata()->bbox().height() + 2 * score()->style().spatium());
                xml_.tag("system-distance", String::number(sys_dist, 2));
            }

            if (mpc.page_start || mpc.score_start) {
                const double top_sys_dist = get_tenths_from_dots(mm_r1->page_pos().y()) - tm;
                xml_.tag("top-system-distance", String::number(top_sys_dist, 2));
            }

            xml_.end_element();
        }

        // Staff layout elements.
        for (size_t staff_idx = (first_staff_of_part == 0) ? 1 : 0; staff_idx < nr_staves_in_part; ++staff_idx) {
            // calculate distance between this and previous staff using the bounding boxes
            const staff_idx_t staff_nr = first_staff_of_part + staff_idx;
            const staff_idx_t prev_staff_nr = system->prev_visible_staff(staff_nr);
            if (prev_staff_nr == muse::nidx) {
                continue;
            }
            if (!system->staff(staff_nr)->show()) {
                hidden_staves_.push_back(staff_idx);
                continue;
            }
            const RectF &prev_bbox = system->staff(prev_staff_nr)->bbox();
            const double staff_dist = system->staff(staff_nr)->bbox().y() - prev_bbox.y() - prev_bbox.height();

            if (staff_dist > 0) {
                xml_.start_element("staff-layout", {{"number", staff_idx + 1}});
                xml_.tag("staff-distance", String::number(get_tenths_from_dots(staff_dist), 2));
                xml_.end_element();
            } else {
                hidden_staves_.push_back(staff_idx);
            }
        }

        // Measure layout elements.
        if (m->prev() && m->prev()->is_h_box()) {
            measure_layout(m->prev()->width());
        }

        xml_.end_element();
    } else if (!attributes.empty()) {
        xml_.tag("print", attributes);
    } else if (m->prev() && m->prev()->is_h_box()) {
        xml_.start_element("print");
        measure_layout(m->prev()->width());
        xml_.end_element();
    }
}

//---------------------------------------------------------
//  measureLayout
//---------------------------------------------------------

void ExportMusicXml::measure_layout(const double distance) {
    xml_.start_element("measure-layout");
    xml_.tag("measure-distance", String::number(get_tenths_from_dots(distance), 2));
    xml_.end_element();
}

//---------------------------------------------------------
//  exportDefaultClef
//---------------------------------------------------------

/**
 In case no clef is found, export a default clef with type determined by staff type.
 Note that a multi-measure rest starting in the first measure should be handled correctly.
 */

void ExportMusicXml::export_default_clef(const Part *const part, const Measure *const m) {
    const size_t staves = part->nstaves();

    if (m->tick() == Fraction(0, 1)) {
        const Segment *clef_seg = m->find_segment(SegmentType::HEADER_CLEF, Fraction(0, 1));

        if (clef_seg) {
            const track_idx_t part_start_track = part->track_range().start_track;
            for (size_t i = 0; i < staves; ++i) {
                // sstaff - xml staff number, counting from 1 for this
                // instrument
                // special number 0 -> don’t show staff number in
                // xml output (because there is only one staff)

                size_t sstaff = (staves > 1) ? i + 1 : 0;
                track_idx_t track = part_start_track + ::mu::engraving::voices * i;

                if (clef_seg->element(track) == nullptr) {
                    ClefType ct{ClefType::G};
                    String stafftype;
                    switch (part->staff(i)->staff_type(Fraction(0, 1))->group()) {
                        case StaffGroup::TAB:
                            ct = ClefType::TAB;
                            stafftype = u"tab";
                            break;
                        case StaffGroup::STANDARD:
                            ct = ClefType::G;
                            stafftype = u"std";
                            break;
                        case StaffGroup::PERCUSSION:
                            ct = ClefType::PERC;
                            stafftype = u"perc";
                            break;
                    }
                    LOGD("no clef found in first measure track %zu (stafftype %s)", track, muPrintable(stafftype));
                    clef(sstaff, ct, u" print-object=\"no\"");
                }
            }
        }
    }
}

//---------------------------------------------------------
//  findAndExportClef
//---------------------------------------------------------

/**
 Make sure clefs at end of measure get exported at start of next measure.
 */

void ExportMusicXml::find_and_export_clef(const Measure *const m, const int staves, const track_idx_t strack, const track_idx_t etrack) {
    Measure *prev_measure = m->prev_measure();
    Measure *mm_r = m->mm_rest(); // the replacing measure in a multi-measure rest
    Fraction tick = m->tick();
    Segment *cs1;
    Segment *cs2 = m->find_segment(SegmentType::CLEF, tick);
    Segment *cs3;
    Segment *seg = 0;

    if (prev_measure) {
        cs1 = prev_measure->find_segment(SegmentType::CLEF, tick);
    } else {
        cs1 = m->find_segment(SegmentType::HEADER_CLEF, tick);
    }

    if (mm_r) {
        cs3 = mm_r->find_segment(SegmentType::HEADER_CLEF, tick);
        if (!cs3) {
            cs3 = mm_r->find_segment(SegmentType::CLEF, tick);
        }
    } else {
        cs3 = 0;
    }

    if (cs1 && cs2) {
        // should only happen at begin of new system
        // when previous system ends with a non-generated clef
        seg = cs1;
    } else if (cs1) {
        seg = cs1;
    } else if (cs3) {
        // happens when the first measure is a multi-measure rest
        // containing a generated clef
        seg = cs3;
    } else {
        seg = cs2;
    }
    clefDebug("exportxml: clef segments cs1=%p cs2=%p cs3=%p seg=%p", cs1, cs2, cs3, seg);

    // output attribute at start of measure: clef
    if (seg) {
        for (track_idx_t st = strack; st < etrack; st += ::mu::engraving::voices) {
            // sstaff - xml staff number, counting from 1 for this
            // instrument
            // special number 0 -> don’t show staff number in
            // xml output (because there is only one staff)

            staff_idx_t sstaff = (staves > 1) ? st - strack + ::mu::engraving::voices : 0;
            sstaff /= ::mu::engraving::voices;

            Clef *cle = to_clef(seg->element(st));
            if (cle) {
                clefDebug("exportxml: clef at start measure ti=%d ct=%d gen=%d", tick, int(cle->clef_type()), cle->generated());
                // output only clef changes, not generated clefs at line beginning
                // exception: at tick=0, export clef anyway
                if ((tick.is_zero() || !cle->generated()) &&
                    ((seg->measure() != m) || ((seg->segment_type() == SegmentType::HEADER_CLEF) && !cle->other_clef()))) {
                    clefDebug("exportxml: clef exported");
                    String clef_attr = color2xml(cle);
                    if (!cle->visible() || (cle->staff() && !cle->staff()->staff_type(cle->tick())->gen_clef())) {
                        clef_attr += u" print-object=\"no\"";
                    }
                    clef(sstaff, cle->clef_type(), clef_attr);
                } else {
                    clefDebug("exportxml: clef not exported");
                }
            }
        }
    }
}

//---------------------------------------------------------
//  findPitchesUsed
//---------------------------------------------------------

/**
 Find the set of pitches actually used in a part.
 */

typedef std::set<int> pitchSet; // the set of pitches used

static void add_chord_pitches_to_set(const Chord *c, pitchSet &set) {
    for (const Note *note : c->notes()) {
        LOGD("chord %p note %p pitch %d", c, note, note->pitch() + 1);
        set.emplace(note->pitch());
    }
}

static void find_pitches_used(const Part *part, pitchSet &set) {
    const TrackRange track_range = part->track_range();

    // loop over all chords in the part
    for (const MeasureBase *mb = part->score()->measures()->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        const Measure *m = to_measure(mb);
        for (track_idx_t st = track_range.start_track; st < track_range.end_track; ++st) {
            for (Segment *seg = m->first(); seg; seg = seg->next()) {
                const EngravingItem *el = seg->element(st);
                if (!el) {
                    continue;
                }
                if (el->is_chord()) {
                    // add grace and non-grace note pitches to the result set
                    const Chord *c = to_chord(el);
                    if (c) {
                        for (const Chord *g : c->grace_notes_before()) {
                            add_chord_pitches_to_set(g, set);
                        }
                        add_chord_pitches_to_set(c, set);
                        for (const Chord *g : c->grace_notes_after()) {
                            add_chord_pitches_to_set(g, set);
                        }
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//  partList
//---------------------------------------------------------

/**
 Write the part list to \a xml.
 */

static void part_list(XmlWriter &xml, Score *score, MusicXmlInstrumentMap &instr_map) {
    const auto &parts = score->parts();
    const Color long_instrument_color = score->style().style_v(Sid::LONG_INSTRUMENT_COLOR).value<Color>();
    const Color short_instrument_color = score->style().style_v(Sid::SHORT_INSTRUMENT_COLOR).value<Color>();
    bool hidden_instr_name = score->style().style_b(Sid::HIDE_INSTRUMENT_NAME_IF_ONE_INSTRUMENT) && parts.size() == 1;

    xml.start_element("part-list");
    size_t staff_count = 0; // count sum of # staves in parts
    const bool group_time = score->style().style_v(Sid::TIME_SIG_PLACEMENT).value<TimeSigPlacement>() == TimeSigPlacement::ACROSS_STAVES;
    std::array<int, ::mu::iex::musicxml::max_part_groups>
        part_group_end; // staff where part group ends (bracketSpan is in staves, not parts)
    part_group_end.fill(-1);
    for (size_t idx = 0; idx < parts.size(); ++idx) {
        const Part *part = parts.at(idx);
        bool bracket_found = false;
        // handle brackets
        for (size_t i = 0; i < part->nstaves(); i++) {
            Staff *st = part->staff(i);
            if (!st) {
                continue;
            }
            const staff_idx_t staff_idx = st->idx();
            for (size_t j = 0; j < score->bracket_levels(staff_idx) + 1; j++) {
                if (score->bracket_type(staff_idx, j) == BracketType::NO_BRACKET) {
                    continue;
                }
                bracket_found = true;
                if (i != 0) {
                    // bracket in other staff not supported in MusicXML
                    LOGD("bracket starting in staff %zu not supported", i + 1);
                    continue;
                }
                // OK, found bracket in first staff of part
                // filter out implicit brackets
                if (score->bracket_span(staff_idx, j) == part->nstaves() && score->bracket_type(staff_idx, j) == BracketType::BRACE) {
                    continue;
                }
                // filter out brackets starting in the last part
                // as they cannot span multiple parts
                if (idx < parts.size() - 1) {
                    // add others
                    int number = find_part_group_number(part_group_end);
                    if (number < ::mu::iex::musicxml::max_part_groups) {
                        const BracketItem *bi = score->brackets(staff_idx).at(j);
                        part_group_start(xml, number + 1, bi, st->bar_line_span(), group_time);
                        part_group_end[number] = static_cast<int>(staff_count + score->bracket_span(staff_idx, j));
                    }
                }
            }
        }
        // handle bracket none
        if (!bracket_found && part->nstaves() > 1) {
            int number = find_part_group_number(part_group_end);
            if (number < ::mu::iex::musicxml::max_part_groups) {
                const BracketItem *bi = Factory::create_bracket_item(score->dummy());
                part_group_start(xml, number + 1, bi, false, group_time);
                delete bi;
                part_group_end[number] = static_cast<int>(idx + part->nstaves());
            }
        }

        xml.start_element_raw(String(u"score-part id=\"P%1\"").arg(idx + 1));
        init_instr_map(instr_map, part->instruments(), score);
        static const std::wregex acc(L"[♭♯]");
        XmlWriter::Attributes long_instrument_attributes;
        XmlWriter::Attributes short_instrument_attributes;
        // by default export the parts long name as part-name
        String part_name = part->long_name();
        // use the track name if no part long name
        if (part_name.empty()) {
            part_name = part->part_name();
            if (!part_name.empty()) {
                hidden_instr_name = true;
            }
        }
        if (hidden_instr_name) {
            long_instrument_attributes.push_back({"print-object", "no"});
            short_instrument_attributes.push_back({"print-object", "no"});
        }
        if (long_instrument_color != score->configuration()->default_color()) {
            long_instrument_attributes.push_back({"color", String::from_std_string(long_instrument_color.to_string())});
        }
        xml.tag("part-name", long_instrument_attributes,
                MScoreTextToMusicXml::to_plain_text(part_name).replace(u'♭', 'b').replace(u'♯', '#'));
        if (part_name.contains(acc)) {
            xml.start_element("part-name-display", long_instrument_attributes);
            write_display_name(xml, part_name);
            xml.end_element();
        }
        if (!part->short_name().is_empty()) {
            if (short_instrument_color != score->configuration()->default_color()) {
                short_instrument_attributes.push_back({"color", String::from_std_string(short_instrument_color.to_string())});
            }
            xml.tag("part-abbreviation", short_instrument_attributes,
                    MScoreTextToMusicXml::to_plain_text(part->short_name()).replace(u'♭', 'b').replace(u'♯', '#'));
            if (part->short_name().contains(acc)) {
                xml.start_element("part-abbreviation-display", short_instrument_attributes);
                write_display_name(xml, part->short_name());
                xml.end_element();
            }
        }

        if (part->instrument()->use_drumset()) {
            const Drumset *drumset = part->instrument()->drumset();
            pitchSet pitches;
            find_pitches_used(part, pitches);
            for (int i = 0; i < 128; ++i) {
                DrumInstrument di = drumset->drum(i);
                if (di.notehead != NoteHeadGroup::HEAD_INVALID) {
                    score_instrument(xml, idx + 1, i + 1, di.name);
                } else if (muse::contains(pitches, i)) {
                    score_instrument(xml, idx + 1, i + 1, String(u"Instrument %1").arg(i + 1));
                }
            }
            int midi_port = part->midi_port() + 1;
            if (midi_port >= 1 && midi_port <= 16) {
                xml.tag("midi-device", {{"port", midi_port}}, "");
            }

            for (int i = 0; i < 128; ++i) {
                DrumInstrument di = drumset->drum(i);
                if (di.notehead != NoteHeadGroup::HEAD_INVALID || muse::contains(pitches, i)) {
                    midi_instrument(xml, idx + 1, i + 1, part->instrument(), score, i + 1);
                }
            }
        } else {
            MusicXmlReverseInstrumentMap rim;
            init_reverse_instr_map(rim, instr_map);

            for (const auto &[instr_nr, instr] : rim) {
                score_instrument(xml, idx + 1, instr_nr + 1, MScoreTextToMusicXml::to_plain_text(instr->track_name()), instr);
            }

            for (auto ii = rim.cbegin(); ii != rim.cend(); ii++) {
                int inst_nr = ii->first;
                int midi_port = part->midi_port() + 1;
                if (ii->second->channel().size() > 0) {
                    midi_port = score->master_score()->midi_mapping(ii->second->channel(0)->channel())->port() + 1;
                }
                if (midi_port >= 1 && midi_port <= 16) {
                    xml.tag_raw(String(u"midi-device %1 port=\"%2\"").arg(instr_id(idx + 1, inst_nr + 1)).arg(midi_port), "");
                } else {
                    xml.tag_raw(String(u"midi-device %1").arg(instr_id(idx + 1, inst_nr + 1)), "");
                }
                midi_instrument(xml, idx + 1, inst_nr + 1, rim.at(inst_nr), score);
            }
        }

        xml.end_element();
        staff_count += part->nstaves();
        for (int i = ::mu::iex::musicxml::max_part_groups - 1; i >= 0; i--) {
            int end = part_group_end[i];
            if (end >= 0) {
                if (static_cast<int>(staff_count) >= end) {
                    xml.tag("part-group", {{"type", "stop"}, {"number", i + 1}});
                    part_group_end[i] = -1;
                }
            }
        }
    }
    xml.end_element();
}

//---------------------------------------------------------
//  tickIsInMiddleOfMeasure
//---------------------------------------------------------

static bool tick_is_in_middle_of_measure(const Fraction ti, const Measure *m) { return ti != m->tick() && ti != m->end_tick(); }

//---------------------------------------------------------
//  writeElement
//---------------------------------------------------------

/**
 Write \a el.
 */

void ExportMusicXml::write_element(EngravingItem *el, const Measure *m, staff_idx_t sstaff, bool use_drumset) {
    if (el->is_clef()) {
        // output only clef changes, not generated clefs
        // at line beginning
        // also ignore clefs at the start of a measure,
        // these have already been output
        // also ignore clefs at the end of a measure
        // these will be output at the start of the next measure
        const Clef *cle = to_clef(el);
        const Fraction ti = cle->segment()->tick();
        const String visible =
            (!cle->visible() || (cle->staff() && !cle->staff()->staff_type(cle->tick())->gen_clef())) ? u" print-object=\"no\"" : String();
        clefDebug("exportxml: clef in measure ti=%d ct=%d gen=%d", ti, int(cle->clef_type()), el->generated());
        if (el->generated()) {
            clefDebug("exportxml: generated clef not exported");
        } else if (!el->generated() && tick_is_in_middle_of_measure(ti, m)) {
            clef(sstaff, cle->clef_type(), color2xml(cle) + visible);
        } else if (!el->generated() && (ti == m->tick()) && (cle->segment()->segment_type() != SegmentType::HEADER_CLEF)) {
            clef(sstaff, cle->clef_type(), color2xml(cle) + visible + String(u" after-barline=\"yes\""));
        } else {
            clefDebug("exportxml: clef not exported");
        }
    } else if (el->is_chord()) {
        Chord *const c = to_chord(el);
        // ise grace after
        if (c) {
            const auto ll = c->lyrics();
            for (Chord *g : c->grace_notes_before()) {
                chord(g, sstaff, ll, use_drumset);
            }
            chord(c, sstaff, ll, use_drumset);
            for (Chord *g : c->grace_notes_after()) {
                chord(g, sstaff, ll, use_drumset);
            }
        }
    } else if (el->is_rest()) {
        Rest *r = to_rest(el);
        if (!(r->is_gap())) {
            const auto ll = r->lyrics();
            rest(r, sstaff, ll);
        }
    } else if (el->is_bar_line()) {
        const BarLine *barln = to_bar_line(el);
        if (tick_is_in_middle_of_measure(barln->tick(), m)) {
            barline_middle(barln);
        }
    } else if (el->is_key_sig() || el->is_time_sig() || el->is_breath() || el->is_time_tick_anchor()) {
        // handled elsewhere
    } else {
        LOGD("ExportMusicXml::write unknown segment type %s", el->type_name());
    }
}

//---------------------------------------------------------
//  clampMusicXmlOctave
//---------------------------------------------------------

/**
 Clamps octave to min and max value as per MusicXML Schema
 */

static void clamp_music_xml_octave(int &octave) { octave = std::clamp(octave, 0, 9); }

//---------------------------------------------------------
//  writeStaffDetails
//---------------------------------------------------------

/**
 Write the staff details for \a part to \a xml.
 */

static void write_staff_details(XmlWriter &xml, const Part *part, const std::vector<size_t> hidden_staves) {
    const Instrument *instrument = part->instrument();
    const size_t staves = part->nstaves();

    // staff details
    for (size_t i = 0; i < staves; i++) {
        Staff *st = part->staff(i);
        const double mag = st->staff_mag(Fraction(0, 1));
        bool hidden = false;
        if (!st->show()) {
            hidden = true;
        } else {
            for (size_t staff_idx : hidden_staves) {
                if (i == staff_idx) {
                    hidden = true;
                }
            }
        }
        const Color line_color = st->color(Fraction(0, 1));
        const bool invis = st->is_lines_invisible(Fraction(0, 1));
        const bool needs_line_details = invis || line_color != part->score()->configuration()->default_color();
        if (st->lines(Fraction(0, 1)) != 5 || st->is_tab_staff(Fraction(0, 1)) || !muse::real_is_equal(mag, 1.0) || hidden ||
            needs_line_details) {
            XmlWriter::Attributes attributes;
            if (staves > 1) {
                attributes.emplace_back(std::make_pair("number", i + 1));
            }
            if (st->is_tab_staff(Fraction(0, 1)) && !st->staff_type(Fraction(0, 1))->use_numbers()) {
                attributes.emplace_back(std::make_pair("show-frets", "letters"));
            }
            if (hidden) {
                attributes.emplace_back(std::make_pair("print-object", "no"));
                if (st->cutaway()) {
                    attributes.emplace_back(std::make_pair("print-spacing", "yes"));
                }
            }

            xml.start_element("staff-details", attributes);

            if (i > 0 && st->links() && st->links()->contains(part->staff(i - 1))) {
                xml.tag("staff-type", "alternate");
            }

            xml.tag("staff-lines", st->lines(Fraction(0, 1)));
            if (needs_line_details) {
                for (int line_idx = 0; line_idx < st->lines(Fraction(0, 1)); ++line_idx) {
                    String ld = String(u"line-detail line=\"%1\"").arg(line_idx + 1);
                    if (line_color != part->score()->configuration()->default_color()) {
                        ld += String(u" color=\"%1\"").arg(String::from_std_string(line_color.to_string()));
                    }
                    if (invis) {
                        ld += u" print-object=\"no\"";
                    }
                    xml.tag_raw(ld);
                }
            }

            if (st->is_tab_staff(Fraction(0, 1)) && instrument->string_data()) {
                std::vector<InstrString> l = instrument->string_data()->string_list();
                for (size_t ii = 0; ii < l.size(); ii++) {
                    char16_t step = u' ';
                    int alter = 0;
                    int octave = 0;
                    midipitch2xml(l.at(ii).pitch, step, alter, octave);
                    xml.start_element("staff-tuning", {{"line", ii + 1}});
                    xml.tag("tuning-step", String(Char(step)));
                    if (alter) {
                        xml.tag("tuning-alter", alter);
                    }
                    clamp_music_xml_octave(octave);
                    xml.tag("tuning-octave", octave);
                    xml.end_element();
                }
            }

            double line_distance = st->line_distance(Fraction(0, 1));
            bool need_write_line_distance = !muse::real_is_equal(line_distance, 1.0);
            bool need_write_mag = !muse::real_is_equal(mag, 1.0);
            if (need_write_line_distance || need_write_mag) {
                XmlWriter::Attributes scale_attributes;
                if (need_write_mag) {
                    scale_attributes.emplace_back(std::make_pair("scaling", mag * 100));
                }
                xml.element("staff-size", scale_attributes, mag * line_distance * 100);
            }

            xml.end_element();
        }
    }
}

//---------------------------------------------------------
//  writeInstrumentChange
//---------------------------------------------------------

/**
 Write the instrument change.
 */

void ExportMusicXml::write_instrument_change(const InstrumentChange *instr_change) {
    const Instrument *instr = instr_change->instrument();
    const Part *part = instr_change->part();
    const size_t part_nr = muse::index_of(score_->parts(), part);
    const int inst_nr = muse::value(instr_map_, instr, -1);
    const String long_name = instr->name_as_plain_text();
    const String short_name = instr->abbreviature_as_plain_text();

    // Instrument changes could happen anywhere in the measure, so we close the initial attributes in any case
    attr_.stop(xml_);
    xml_.start_element("print");
    if (!long_name.is_empty()) {
        xml_.start_element("part-name-display");
        write_display_name(xml_, long_name);
        xml_.end_element();
    }
    if (!short_name.is_empty()) {
        xml_.start_element("part-abbreviation-display");
        write_display_name(xml_, short_name);
        xml_.end_element();
    }
    xml_.end_element();

    write_instrument_details(instr, score_->style().style_b(Sid::CONCERT_PITCH));

    xml_.start_element("sound");
    if (!instr->music_xml_id().empty()) {
        xml_.start_element_raw(String(u"instrument-change %1").arg(instr_id(static_cast<int>(part_nr) + 1, inst_nr + 1)));
        xml_.tag("instrument-sound", instr->music_xml_id());
        xml_.end_element();
    } else {
        xml_.tag_raw(String(u"instrument-change %1").arg(instr_id(static_cast<int>(part_nr) + 1, inst_nr + 1)));
    }
    xml_.end_element();
}

//---------------------------------------------------------
//  writeInstrumentDetails
//---------------------------------------------------------

/**
 Write the instrument details for \a instrument.
 */

void ExportMusicXml::write_instrument_details(const Instrument *instrument, const bool concert_pitch) {
    if (instrument->transpose().chromatic) {
        attr_.do_attr(xml_, true);
        if (concert_pitch) {
            xml_.start_element("for-part");
            xml_.start_element("part-transpose");
        } else {
            xml_.start_element("transpose");
        }
        xml_.tag("diatonic", instrument->transpose().diatonic % 7);
        xml_.tag("chromatic", instrument->transpose().chromatic % 12);
        const int octave_change = instrument->transpose().chromatic / 12;
        if (octave_change) {
            xml_.tag("octave-change", octave_change);
        }
        xml_.end_element();
        if (concert_pitch) {
            xml_.end_element();
        }
        attr_.do_attr(xml_, false);
    }
}

//---------------------------------------------------------
//  annotationsWithoutNote
//---------------------------------------------------------

/**
 Write the annotations that could not be attached to notes.
 */

static void annotations_without_note(ExportMusicXml *exp, const track_idx_t strack, const int staves, const Measure *const measure) {
    for (Segment *segment = measure->first(); segment; segment = segment->next()) {
        if (segment->segment_type() == SegmentType::CHORD_REST) {
            for (const EngravingItem *element : segment->annotations()) {
                if (!element->is_figured_bass() && !element->is_harmony()) { // handled elsewhere
                    if (!exp->can_write(element)) {
                        continue;
                    }
                    const track_idx_t wtrack = find_track_for_annotations(element->track(), segment); // track to write annotation
                    if (strack <= element->track() && element->track() < (strack + ::mu::engraving::voices * staves) &&
                        wtrack == muse::nidx) {
                        common_annotations(exp, element, staves > 1 ? 1 : 0);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------
//  MeasureNumberStateHandler
//---------------------------------------------------------

MeasureNumberStateHandler::MeasureNumberStateHandler() { init(); }

void MeasureNumberStateHandler::init() {
    measure_number_ = 1;
    measure_number_offset_ = 0;
    irregular_measure_number_ = 1;
    pickup_measure_number_ = 1;
}

void MeasureNumberStateHandler::update_for_measure(const Measure *const m) {
    // restart measure numbering after a section break if startWithMeasureOne is set
    // check the previous MeasureBase instead of Measure to catch breaks in frames too
    const MeasureBase *previous_mb = m->prev();
    if (previous_mb) {
        previous_mb = previous_mb->mb_with_preceding_section_break();
    }

    if (previous_mb) {
        const LayoutBreak *layout_section_break = previous_mb->section_break_element();
        if (layout_section_break && layout_section_break->start_with_measure_one()) {
            init();
        }
    }

    // update measure numbers and cache result
    measure_number_offset_ = m->measure_number_offset();
    measure_number_ += measure_number_offset_;
    cached_attributes_ = u" number=";
    if ((irregular_measure_number_ + measure_number_) == 2 && m->exclude_from_numbering()) {
        cached_attributes_ += u"\"0\" implicit=\"yes\"";
        pickup_measure_number_++;
    } else if (m->exclude_from_numbering()) {
        cached_attributes_ += String(u"\"X%1\" implicit=\"yes\"").arg(irregular_measure_number_++);
    } else {
        cached_attributes_ += String(u"\"%1\"").arg(measure_number_++);
    }
}

String MeasureNumberStateHandler::measure_number() const { return cached_attributes_; }

bool MeasureNumberStateHandler::is_first_actual_measure() const {
    return (irregular_measure_number_ + (measure_number_ - measure_number_offset_) + pickup_measure_number_) == 4;
}

//---------------------------------------------------------
//  findLastSystemWithMeasures
//---------------------------------------------------------

static System *find_last_system_with_measures(const Page *const page) {
    for (int i = static_cast<int>(page->systems().size()) - 1; i >= 0; --i) {
        System *const s = page->systems().at(i);
        const Measure *m = s->first_measure();
        if (m) {
            return s;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//  isFirstMeasureInSystem
//---------------------------------------------------------

static bool is_first_measure_in_system(const Measure *const measure) {
    const System *system = measure->covering_mm_rest_or_this()->system();
    const Measure *first_measure_in_system = system->first_measure();
    const Measure *real_first_measure_in_system =
        first_measure_in_system->is_mm_rest() ? first_measure_in_system->mm_rest_first() : first_measure_in_system;
    return measure == real_first_measure_in_system;
}

//---------------------------------------------------------
//  isFirstMeasureInLastSystem
//---------------------------------------------------------

static bool is_first_measure_in_last_system(const Measure *const measure) {
    const System *system = measure->covering_mm_rest_or_this()->system();
    const Page *page = system->page();

    /*
     Notes on multi-measure rest handling:
     Function mmRest1() returns either the measure itself (if not part of multi-measure rest)
     or the replacing multi-measure rest measure.
     Using this is required as a measure that is covered by a multi-measure rest has no system.
     Furthermore, the first measure in a system starting with a multi-measure rest is the a multi-
     measure rest itself instead of the first covered measure.
     */

    const System *last_system = find_last_system_with_measures(page);
    if (!last_system) {
        return false; // degenerate case: no system with measures found
    }
    const Measure *first_measure_in_last_system = last_system->first_measure();
    const Measure *real_first_measure_in_last_system =
        first_measure_in_last_system->is_mm_rest() ? first_measure_in_last_system->mm_rest_first() : first_measure_in_last_system;
    return measure == real_first_measure_in_last_system;
}

//---------------------------------------------------------
//  systemHasMeasures
//---------------------------------------------------------

static bool system_has_measures(const System *const system) { return system->first_measure(); }

//---------------------------------------------------------
//  findTextFramesToWriteAsWordsAbove
//---------------------------------------------------------

static std::vector<TBox *> find_text_frames_to_write_as_words_above(const Measure *const measure) {
    const System *system = measure->covering_mm_rest_or_this()->system();
    const Page *page = system->page();
    const size_t system_index = muse::index_of(page->systems(), system);
    std::vector<TBox *> tboxes;
    if (is_first_measure_in_system(measure)) {
        for (int idx = static_cast<int>(system_index - 1); idx >= 0 && !system_has_measures(page->system(idx)); --idx) {
            const System *sys = page->system(idx);
            for (MeasureBase *mb : sys->measures()) {
                if (mb->is_t_box()) {
                    TBox *const tbox = to_t_box(mb);
                    tboxes.insert(tboxes.begin(), tbox);
                }
            }
        }
    }
    return tboxes;
}

//---------------------------------------------------------
//  findTextFramesToWriteAsWordsBelow
//---------------------------------------------------------

static std::vector<TBox *> find_text_frames_to_write_as_words_below(const Measure *const measure) {
    const System *system = measure->covering_mm_rest_or_this()->system();
    const Page *page = system->page();
    const size_t system_index = static_cast<int>(muse::index_of(page->systems(), system));
    std::vector<TBox *> tboxes;
    if (is_first_measure_in_last_system(measure)) {
        for (size_t idx = system_index + 1; idx < page->systems().size() /* && !systemHasMeasures(page->system(idx))*/; ++idx) {
            const System *sys = page->system(static_cast<int>(idx));
            for (MeasureBase *mb : sys->measures()) {
                if (mb->is_t_box()) {
                    TBox *const tbox = to_t_box(mb);
                    tboxes.insert(tboxes.begin(), tbox);
                }
            }
        }
    }
    return tboxes;
}

//---------------------------------------------------------
//  writeMeasureTracks
//---------------------------------------------------------

/**
 Write data contained in the measure's tracks.
 */

void ExportMusicXml::write_measure_tracks(const Measure *const m, const int part_index, const staff_idx_t strack,
                                          const staff_idx_t part_rel_staff_no, const bool use_drumset, const bool is_last_staff_of_part,
                                          FigBassMap &fb_map, std::set<const Spanner *> &spanners_stopped) {
    const auto tboxes_above = find_text_frames_to_write_as_words_above(m);
    const auto tboxes_below = find_text_frames_to_write_as_words_below(m);

    track_idx_t etrack = strack + ::mu::engraving::voices;
    for (track_idx_t track = strack; track < etrack; ++track) {
        for (Segment *seg = m->first(); seg; seg = seg->next()) {
            if (seg->is_time_tick_type()) {
                // Prefer to start/stop spanners on a chordrest segment where one is available
                const Segment *cr_seg = score_->tick2left_segment(seg->tick());
                if (cr_seg && cr_seg->tick() == seg->tick()) {
                    bool staff_has_cr = false;
                    for (track_idx_t t = strack; t < etrack; ++t) {
                        if (cr_seg->element(t)) {
                            staff_has_cr = true;
                            break;
                        }
                    }
                    if (staff_has_cr) {
                        continue;
                    }
                }
                spanner_start(this, strack, etrack, track, part_rel_staff_no, seg);

                const staff_idx_t spanner_staff = track2staff(track);
                const track_idx_t starttrack = staff2track(spanner_staff);
                const track_idx_t endtrack = staff2track(spanner_staff + 1);
                spanner_stop(this, starttrack, endtrack, seg->tick(), part_rel_staff_no, spanners_stopped);

                // We check if there are additional annotations
                for (EngravingItem *annotation : seg->annotations()) {
                    if (annotation->track() != track || !annotation->is_text_base()) {
                        continue;
                    }
                    // Just to include them
                    annotations(this, strack, etrack, track, part_rel_staff_no, seg);
                    break;
                }
                continue;
            }
            EngravingItem *const el = seg->element(track);
            if (!el) {
                continue;
            }
            // must ignore start repeat to prevent spurious backup/forward
            if (el->is_bar_line() && to_bar_line(el)->bar_line_type() == BarLineType::START_REPEAT) {
                continue;
            }

            // generate backup or forward to the start time of the element
            move_to_tick_if_need(seg->tick(), track, m->tick());

            EngravingItem *ic = seg->find_annotation(ElementType::INSTRUMENT_CHANGE, strack, etrack - 1);
            if (ic && (track == strack)) {
                const InstrumentChange *instr_change = to_instrument_change(ic);
                write_instrument_change(instr_change);
            }

            // handle annotations and spanners (directions attached to this note or rest)
            if (el->is_chord_rest()) {
                attr_.do_attr(xml_, false);
                const bool is_first_part = (part_index == 0);
                const bool is_last_part = (part_index == (static_cast<int>(score_->parts().size()) - 1));
                if (!tboxes_above_written_ && is_first_part) {
                    for (const TBox *tbox : tboxes_above) {
                        // note: use mmRest1() to get at a possible multi-measure rest,
                        // as the covered measure would be positioned at 0,0.
                        tbox_text_as_words(tbox->text(), 0,
                                           muse::PointF(tbox->text()->canvas_pos() - m->covering_mm_rest_or_this()->canvas_pos()));
                    }
                    tboxes_above_written_ = true;
                }
                if (!tboxes_below_written_ && is_last_part && is_last_staff_of_part) {
                    for (const TBox *tbox : tboxes_below) {
                        const staff_idx_t last_staff_nr = track2staff(track);
                        const System *sys = m->covering_mm_rest_or_this()->system();
                        PointF text_pos = tbox->text()->canvas_pos() - m->covering_mm_rest_or_this()->canvas_pos();
                        if (last_staff_nr < sys->staves().size()) {
                            // convert to position relative to last staff of system
                            text_pos.set_y(text_pos.y() - (sys->staff_canvas_ypage(last_staff_nr) - sys->staff_canvas_ypage(0)));
                        }
                        tbox_text_as_words(tbox->text(), part_rel_staff_no, text_pos);
                    }
                    tboxes_below_written_ = true;
                }
                harmonies(this, track, seg);
                annotations(this, strack, etrack, track, part_rel_staff_no, seg);
                figured_bass(xml_, strack, etrack, track, to_chord_rest(el), fb_map, div_);
                spanner_start(this, strack, etrack, track, part_rel_staff_no, seg);
            }

            // write element el if necessary
            write_element(el, m, part_rel_staff_no, use_drumset);

            // handle annotations and spanners (directions attached to this note or rest)
            if (el->is_chord_rest()) {
                const staff_idx_t spanner_staff = track2staff(track);
                const track_idx_t starttrack = staff2track(spanner_staff);
                const track_idx_t endtrack = staff2track(spanner_staff + 1);
                spanner_stop(this, starttrack, endtrack, tick_, part_rel_staff_no, spanners_stopped);
            }
        } // for (Segment* seg = ...
        attr_.stop(xml_);
    } // for (int st = ...
}

//---------------------------------------------------------
//  writeMeasureStaves
//---------------------------------------------------------

/**
 Write each staff of a measure for a given part.
 */

void ExportMusicXml::write_measure_staves(const Measure *m, const int part_index, const staff_idx_t start_staff, const size_t nstaves,
                                          const bool use_drumset, FigBassMap &fb_map, std::set<const Spanner *> &spanners_stopped) {
    const staff_idx_t end_staff = start_staff + nstaves;
    const Measure *const orig_m = m;
    tboxes_above_written_ = false;
    tboxes_below_written_ = false;
    meas_arpeggios_.clear();

    for (staff_idx_t staff_idx = start_staff; staff_idx < end_staff; ++staff_idx) {
        // some staves may need to make m point somewhere else, so just in case, ensure start in same place
        IF_ASSERT_FAILED(m == orig_m) { return; }
        move_to_tick(m->tick()); // move tick to start of measure

        staff_idx_t part_rel_staff_no =
            (nstaves > 1 ? staff_idx - start_staff + 1 : 0); // xml staff number, counting from 1 for this instrument
        // special number 0 -> don’t show staff number in xml output
        // (because there is only one staff)

        // in presence of a MeasureRepeat, adjust m to point to the actual content being repeated
        if (m->is_measure_repeat_group(staff_idx)) {
            MeasureRepeat *mr = nullptr;
            while (m->is_measure_repeat_group(staff_idx) && m->prev_measure()) { // keep going back until out of measure repeat groups
                mr = m->measure_repeat_element(staff_idx);
                IF_ASSERT_FAILED(mr) {
                    LOGE() << "Could not find MeasureRepeat on measure: " << m->index() << ", staff: " << staff_idx;
                    break;
                }

                for (int i = 0; i < mr->num_measures() && m->prev_measure(); ++i) {
                    m = m->prev_measure();
                }
            }
        }
        // in case m was changed, also rewind _tick so as not to generate unnecessary backup/forward tags
        Fraction tick_delta = tick_ - m->tick();
        tick_ -= tick_delta;

        bool is_last_staff_of_part = (end_staff - 1 <= staff_idx); // for writing tboxes below

        write_measure_tracks(m, part_index, staff2track(staff_idx), part_rel_staff_no, use_drumset, is_last_staff_of_part, fb_map,
                             spanners_stopped);

        // restore m and _tick before advancing to next staff in part
        m = orig_m;
        tick_ += tick_delta;
        if (!is_last_staff_of_part) {
            // move tick to start of measure, generates backup to next staff, needs this staff's stretch
            move_to_tick(m->tick(), stretch(score_, staff_idx * ::mu::engraving::voices, m->tick()));
        }
    }
}

//---------------------------------------------------------
//  writeMeasure
//---------------------------------------------------------

/**
 Write a measure.
 */

void ExportMusicXml::write_measure(const Measure *const m, const int part_index, const int staff_count, MeasureNumberStateHandler &mnsh,
                                   FigBassMap &fb_map, const MeasurePrintContext &mpc, std::set<const Spanner *> &spanners_stopped) {
    const Part *part = score_->parts().at(part_index);
    const size_t staves = part->nstaves();
    const TrackRange track_range = part->track_range();
    const track_idx_t strack = track_range.start_track;
    const track_idx_t etrack = track_range.end_track;

    // If MMRests are enabled, elements are moved from the underlying measure to the convering MMRest measure
    // Make sure we find these elements in the correct measure
    const Measure *mm_rest = m->covering_mm_rest_or_this();
    const Measure *start_measure = mm_rest->is_mm_rest() && mm_rest->mm_rest_first() == m ? mm_rest : m;
    const Measure *end_measure = mm_rest->is_mm_rest() && mm_rest->mm_rest_last() == m ? mm_rest : m;

    // pickup and other irregular measures need special care
    String measure_tag = u"measure";
    mnsh.update_for_measure(m);
    measure_tag += mnsh.measure_number();
    const bool is_first_actual_measure = mnsh.is_first_actual_measure();

    if (configuration()->export_layout()) {
        measure_tag +=
            String(u" width=\"%1\"").arg(String::number(m->ldata()->bbox().width() / ::mu::engraving::dpmm / millimeters_ * tenths_, 2));
    }

    xml_.start_element_raw(measure_tag);

    print(m, part_index, staff_count, staves, mpc);

    attr_.start();

    find_trills(m, strack, etrack, trill_start_, trill_stop_);

    // barline left must be the first element in a measure
    barline_left(start_measure, strack);

    // output attributes with the first actual measure (pickup or regular)
    if (is_first_actual_measure) {
        attr_.do_attr(xml_, true);
        xml_.tag("divisions", div_);
    }

    // output attributes at start of measure: key, time
    keysig_timesig(start_measure, part);

    // output attributes with the first actual measure (pickup or regular) only
    if (is_first_actual_measure) {
        if (staves > 1) {
            xml_.tag("staves", static_cast<int>(staves));
        }

        if (instr_map_.size() > 1) {
            xml_.tag("instruments", instr_map_.size());
        }
    }

    // make sure clefs at end of measure get exported at start of next measure
    find_and_export_clef(start_measure, static_cast<int>(staves), strack, etrack);

    // make sure a clef gets exported if none is found
    export_default_clef(part, start_measure);

    // output attributes with the first actual measure (pickup or regular) only
    if (is_first_actual_measure) {
        write_staff_details(xml_, part, hidden_staves_);
        write_instrument_details(part->instrument(), score_->style().style_b(Sid::CONCERT_PITCH));
    } else {
        for (size_t staff_idx : hidden_staves_) {
            attr_.do_attr(xml_, true);
            XmlWriter::Attributes attributes;
            if (staves > 1) {
                attributes.emplace_back(std::make_pair("number", staff_idx + 1));
            }
            attributes.emplace_back(std::make_pair("print-object", "no"));
            if (part->staff(staff_idx)->cutaway()) {
                attributes.emplace_back(std::make_pair("print-spacing", "yes"));
            }
            xml_.tag("staff-details", attributes);
            attr_.do_attr(xml_, false);
        }
    }
    hidden_staves_.clear();

    // output attribute at start of measure: measure-style
    measure_style(xml_, attr_, m, part_index);

    // MuseScore limitation: repeats are always in the first part
    // and are implicitly placed at either measure start or stop
    if (part_index == 0) {
        repeat_at_measure_start(attr_, start_measure, strack, etrack, strack);
    }

    if (start_measure->is_mm_rest()) {
        // Write annotations on first chordrest
        // rehearsal marks etc.
        Segment *annotation_seg = start_measure->find_segment_r(SegmentType::CHORD_REST, Fraction(0, 1));
        if (annotation_seg) {
            for (track_idx_t track = strack; track < etrack; ++track) {
                const staff_idx_t xml_staff = staves > 1 ? (track2staff(track) - track2staff(strack) + 1) : 0;
                annotations(this, strack, etrack, track, xml_staff, annotation_seg);
            }
        }
    }

    // write data in the staves
    write_measure_staves(m, part_index, track2staff(strack), staves, part->instrument()->use_drumset(), fb_map, spanners_stopped);

    // write the annotations that could not be attached to notes
    annotations_without_note(this, strack, static_cast<int>(staves), start_measure);

    // move to end of measure (in case of incomplete last voice)
#ifdef DEBUG_TICK
    LOGD("end of measure");
#endif
    move_to_tick(m->end_tick(), stretch(score_, etrack - 1, m->tick()));
    if (part_index == 0) {
        repeat_at_measure_stop(end_measure, strack, etrack, strack);
    }
    // note: don't use "m->repeatFlags() & Repeat::END" here, because more
    // barline types need to be handled besides repeat end ("light-heavy")
    barline_right(end_measure, strack, etrack);
    xml_.end_element();
}

//---------------------------------------------------------
//  measureWritten
//---------------------------------------------------------

void MeasurePrintContext::measure_written(const Measure *m) {
    score_start = false;
    page_start = false;
    system_start = false;
    prev_measure = m;
}

bool ExportMusicXml::should_write_page_no(const Page *page) {
    const MStyle &style = score_->style();
    const page_idx_t n = page->page_number() + 1 + score_->page_number_offset();

    auto macro_print_page_no = [&](Sid sid) -> bool {
        String text = style.style_st(sid);
        std::wregex page_no = std::wregex(L"\\$([p|N|P])");
        StringList results = text.search(page_no, {1}, SplitBehavior::SKIP_EMPTY_PARTS);

        for (const String &s : results) {
            Char c = s.at(0);
            if ((c == 'P') || (c == 'p' && page->page_number() > 0) ||
                (c == 'N' && (page->page_number() + score_->page_number_offset() > 0 || score_->npages() > 1))) {
                return true;
            }
        }
        return false;
    };

    if (style.style_b(Sid::SHOW_HEADER) && (page->page_number() || style.style_b(Sid::HEADER_FIRST_PAGE))) {
        const bool odd = (n & 1) || !style.style_b(Sid::HEADER_ODD_EVEN);

        static constexpr Sid odd_headers[] = {Sid::ODD_HEADER_L, Sid::ODD_HEADER_C, Sid::ODD_HEADER_R};
        static constexpr Sid even_headers[] = {Sid::EVEN_HEADER_L, Sid::EVEN_HEADER_C, Sid::EVEN_HEADER_R};

        for (Sid sid : odd ? odd_headers : even_headers) {
            if (macro_print_page_no(sid)) {
                return true;
            }
        }
    }

    if (style.style_b(Sid::SHOW_FOOTER) && (page->page_number() || style.style_b(Sid::FOOTER_FIRST_PAGE))) {
        const bool odd = (n & 1) || !style.style_b(Sid::FOOTER_ODD_EVEN);

        static constexpr Sid odd_footers[] = {Sid::ODD_FOOTER_L, Sid::ODD_FOOTER_C, Sid::ODD_FOOTER_R};
        static constexpr Sid even_footers[] = {Sid::EVEN_FOOTER_L, Sid::EVEN_FOOTER_C, Sid::EVEN_FOOTER_R};

        for (Sid sid : odd ? odd_footers : even_footers) {
            if (macro_print_page_no(sid)) {
                return true;
            }
        }
    }

    return false;
}

//---------------------------------------------------------
//  writeParts
//---------------------------------------------------------

/**
 Write all parts.
 */

void ExportMusicXml::write_parts() {
    int staff_count = 0;
    const auto &parts = score_->parts();

    for (size_t part_index = 0; part_index < parts.size(); ++part_index) {
        const Part *part = parts.at(part_index);
        gh_.reset(); // reset glissando handler state for each part
        tick_ = {0, 1};
        xml_.start_element_raw(String(u"part id=\"P%1\"").arg(part_index + 1));

        trill_start_.clear();
        trill_stop_.clear();
        init_instr_map(instr_map_, part->instruments(), score_);

        MeasureNumberStateHandler mnsh;
        FigBassMap fb_map; // pending figured bass extends

        // set of spanners already stopped in this part
        // required to prevent multiple spanner stops for the same spanner
        std::set<const Spanner *> spanners_stopped;

        const auto &pages = score_->pages();
        MeasurePrintContext mpc;

        for (size_t page_index = 0; page_index < pages.size(); ++page_index) {
            const Page *page = pages.at(page_index);
            mpc.page_start = true;
            mpc.page_number = page->page_number() + 1 + score_->page_number_offset();
            mpc.write_page_no = should_write_page_no(page);
            const auto &systems = page->systems();

            for (int system_index = 0; system_index < static_cast<int>(systems.size()); ++system_index) {
                const System *system = systems.at(system_index);
                mpc.system_start = true;

                for (const MeasureBase *mb : system->measures()) {
                    if (!mb->is_measure()) {
                        continue;
                    }
                    const Measure *m = to_measure(mb);

                    if (m->is_mm_rest()) {
                        // in case of a multimeasure rest (which is a single measure in MuseScore), write the measure range it replaces
                        const Measure *m2 = m->mm_rest_last()->next_measure();
                        for (Measure *m1 = m->mm_rest_first(); m1 != m2; m1 = m1->next_measure()) {
                            if (m1->is_measure()) {
                                write_measure(m1, static_cast<int>(part_index), staff_count, mnsh, fb_map, mpc, spanners_stopped);
                                mpc.measure_written(m1);
                            }
                        }
                    } else {
                        // write the measure (or, if measure repeat, the "underlying" measure that it indicates for the musician to play)
                        write_measure(m, static_cast<int>(part_index), staff_count, mnsh, fb_map, mpc, spanners_stopped);
                        mpc.measure_written(m);
                    }
                }
                mpc.prev_system = system;
            }
            mpc.last_system_prev_page = mpc.prev_system;
        }

        staff_count += static_cast<int>(part->nstaves());
        xml_.end_element();
    }
}

//---------------------------------------------------------
//  findJumpElements
//---------------------------------------------------------

static std::vector<const Jump *> find_jump_elements(const Score *score) {
    std::vector<const Jump *> jumps;

    for (const MeasureBase *m = score->first(); m; m = m->next()) {
        for (const EngravingItem *e : m->el()) {
            if (e->is_jump()) {
                jumps.push_back(to_jump(e));
            }
        }
    }

    return jumps;
}

//---------------------------------------------------------
//  write
//---------------------------------------------------------

/**
 Write the score to \a dev in MusicXML format.
 */

void ExportMusicXml::write(muse::io::IODevice *dev) {
    calc_divisions();

    for (int i = 0; i < ::mu::iex::musicxml::max_number_level; ++i) {
        brackets_[i] = nullptr;
        dashes_[i] = nullptr;
        hairpins_[i] = nullptr;
        ottavas_[i] = nullptr;
        trills_[i] = nullptr;
    }

    jump_elements_ = find_jump_elements(score_);

    xml_.set_device(dev);
    xml_.start_document();
    xml_.write_doctype(
        u"score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\"");

    xml_.start_element("score-partwise", {{"version", "4.0"}});

    work(score_->measures()->first());
    identification(xml_, score_);

    if (configuration()->export_layout()) {
        defaults(xml_, score_->style(), millimeters_, tenths_);
        credits(xml_);
    } else if (score_->style().style_b(Sid::CONCERT_PITCH)) {
        xml_.start_element("defaults");
        xml_.tag("concert-score");
        xml_.end_element();
    }

    part_list(xml_, score_, instr_map_);
    write_parts();

    xml_.end_element();
    xml_.flush();
}

//---------------------------------------------------------
//   saveXml
//    return false on error
//---------------------------------------------------------

/**
 Save Score as MusicXML file \a name.

 Return false on error.
 */

bool save_xml(Score *score, IODevice *device, const IMusicXmlConfiguration &configuration) {
    ExportMusicXml em(score, configuration);
    em.write(device);
    return true;
}

bool save_xml(Score *score, const String &name, const IMusicXmlConfiguration &configuration) {
    auto buf = Buffer::opened(IODevice::WRITE_ONLY);

    bool res = save_xml(score, &buf, configuration) && !buf.has_error();
    if (!res) {
        return res;
    }
    buf.close();

    return File::write_file(name, buf.data());
}

//---------------------------------------------------------
//   saveMxl
//    return false on error
//---------------------------------------------------------

/**
 Save Score as compressed MusicXML file \a name.

 Return false on error.
 */

// META-INF/container.xml:
// <?xml version="1.0" encoding="UTF-8"?>
// <container>
//     <rootfiles>
//         <rootfile full-path="testHello.xml"/>
//     </rootfiles>
// </container>

static void write_mxl_archive(Score *score, muse::ZipWriter &zip, const String &filename, const IMusicXmlConfiguration &configuration) {
    auto cbuf = Buffer::opened(IODevice::READ_WRITE);

    XmlWriter xml(&cbuf);
    xml.start_document();
    xml.start_element("container");
    xml.start_element("rootfiles");
    xml.start_element("rootfile", {{"full-path", filename}});
    xml.end_element();
    xml.end_element();
    xml.end_element();
    xml.flush();
    cbuf.seek(0);

    zip.add_file("META-INF/container.xml", cbuf.data());

    auto dbuf = Buffer::opened(IODevice::READ_WRITE);
    ExportMusicXml em(score, configuration);
    em.set_mxl(true);
    em.write(&dbuf);
    dbuf.seek(0);
    zip.add_file(filename.to_std_string(), dbuf.data());

    for (const ImageStoreItem *isi : image_store) {
        if (isi->is_used(score)) {
            String source;
            String type;
            get_image_info(isi, source, type);
            zip.add_file(source.to_std_string(), isi->buffer());
        }
    }
}

bool save_mxl(Score *score, IODevice *device, const IMusicXmlConfiguration &configuration) {
    muse::ZipWriter zip(device);

    //anonymized filename since we don't know the actual one here
    String fn = u"score.xml";
    write_mxl_archive(score, zip, fn, configuration);
    zip.close();

    return true;
}

bool save_mxl(Score *score, const String &name, const IMusicXmlConfiguration &configuration) {
    auto out_buf = Buffer::opened(IODevice::WRITE_ONLY);
    muse::ZipWriter zip(&out_buf);

    FileInfo fi(name);
    String fn = fi.complete_base_name() + u".xml";
    write_mxl_archive(score, zip, fn, configuration);
    zip.close();

    return File::write_file(name, out_buf.data());
}

double ExportMusicXml::get_tenths_from_inches(double inches) const { return inches * ::mu::engraving::inch / millimeters_ * tenths_; }

double ExportMusicXml::get_tenths_from_dots(double dots) const { return dots / ::mu::engraving::dpmm / millimeters_ * tenths_; }

static void write_music_xml(const FretDiagram *item, XmlWriter &xml) {
    LOGD("FretDiagram::writeMusicXml() this %p harmony %p", item, item->harmony());
    XmlWriter::Attributes frame_attrs;
    add_color_attr(item, frame_attrs);
    xml.start_element("frame", frame_attrs);
    xml.tag("frame-strings", item->strings());
    xml.tag("frame-frets", item->frets());
    if (item->fret_offset() > 0) {
        xml.tag("first-fret", item->fret_offset() + 1);
    }

    for (int i = 0; i < item->strings(); ++i) {
        const int mxml_string = item->strings() - i;

        std::vector<int> b_starts;
        std::vector<int> b_ends;
        for (auto const &j : item->barres()) {
            FretItem::Barre b = j.second;
            const int fret = j.first;
            if (!b.exists()) {
                continue;
            }

            if (b.start_string == i) {
                b_starts.push_back(fret);
            } else if (b.end_string == i || (b.end_string == -1 && mxml_string == 1)) {
                b_ends.push_back(fret);
            }
        }

        if (item->marker(i).exists() && item->marker(i).mtype == FretMarkerType::CIRCLE) {
            xml.start_element("frame-note");
            xml.tag("string", mxml_string);
            xml.tag("fret", "0");
            xml.end_element();
        }
        // Markers may exists alongside with dots
        // Write dots
        for (auto const &d : item->dot(i)) {
            if (!d.exists() || d.dtype == FretDotType::CROSS) {
                continue;
            }
            xml.start_element("frame-note");
            xml.tag("string", mxml_string);
            xml.tag("fret", d.fret + item->fret_offset());
            // TODO: write fingerings

            // Also write barre if it starts at this dot
            if (std::find(b_starts.begin(), b_starts.end(), d.fret) != b_starts.end()) {
                xml.tag("barre", {{"type", "start"}});
                b_starts.erase(std::remove(b_starts.begin(), b_starts.end(), d.fret), b_starts.end());
            }
            if (std::find(b_ends.begin(), b_ends.end(), d.fret) != b_ends.end()) {
                xml.tag("barre", {{"type", "stop"}});
                b_ends.erase(std::remove(b_ends.begin(), b_ends.end(), d.fret), b_ends.end());
            }
            xml.end_element();
        }

        // Write unwritten barres
        for (int j : b_starts) {
            xml.start_element("frame-note");
            xml.tag("string", mxml_string);
            xml.tag("fret", j + item->fret_offset());
            xml.tag("barre", {{"type", "start"}});
            xml.end_element();
        }

        for (int j : b_ends) {
            xml.start_element("frame-note");
            xml.tag("string", mxml_string);
            xml.tag("fret", j + item->fret_offset());
            xml.tag("barre", {{"type", "stop"}});
            xml.end_element();
        }
    }

    xml.end_element();
}

//---------------------------------------------------------
//   harmony
//---------------------------------------------------------

void ExportMusicXml::harmony(Harmony const *const h, FretDiagram const *const fd, const Fraction &offset) {
    if (h->chords().empty()) {
        return;
    }
    XmlWriter::Attributes harmony_attrs;
    if (h->is_polychord()) {
        harmony_attrs.emplace_back(std::make_pair("arrangement", "vertical"));
    }
    if (!h->is_styled(Pid::PLACEMENT)) {
        harmony_attrs.emplace_back(std::make_pair("placement", TConv::to_xml(h->placement())));
    }
    if (!h->is_styled(Pid::FONT_FACE)) {
        harmony_attrs.emplace_back(std::make_pair("font-family", h->get_property(Pid::FONT_FACE).value<String>()));
    }
    if (!h->is_styled(Pid::FONT_SIZE)) {
        harmony_attrs.emplace_back(std::make_pair("font-size", h->get_property(Pid::FONT_SIZE).to_real()));
    }
    harmony_attrs.emplace_back(std::make_pair("print-frame", h->has_frame() ? "yes" : "no")); // .append(relative));
    if (!h->visible()) {
        harmony_attrs.emplace_back(std::make_pair("print-object", "no"));
    }
    add_color_attr(h, harmony_attrs);
    xml_.start_element("harmony", harmony_attrs);
    if (h->harmony_type() == HarmonyType::STANDARD && tpc_is_valid(h->root_tpc())) {
        for (const HarmonyInfo *info : h->chords()) {
            if (!info || !tpc_is_valid(info->root_tpc())) {
                LOGD("invalid HarmonyInfo");
                continue; // skip invalid HarmonyInfo
            }
            const int root_tpc = info->root_tpc();
            const int bass_tpc = info->bass_tpc();
            xml_.start_element("root");
            xml_.tag("root-step", tpc2step_name(root_tpc));
            int alter = int(tpc2alter(root_tpc));
            if (alter) {
                xml_.tag("root-alter", alter);
            }
            xml_.end_element();

            const String xml_kind = harmony_xml_kind(info);

            if (!xml_kind.is_empty()) {
                String s = u"kind";
                String kind_text = harmony_xml_text(info);
                if (!harmony_xml_text(info).empty()) {
                    s += u" text=\"" + kind_text + u"\"";
                }
                if (harmony_xml_symbols(info) == u"yes") {
                    s += u" use-symbols=\"yes\"";
                }
                if (harmony_xml_parens(info) == u"yes") {
                    s += u" parentheses-degrees=\"yes\"";
                }
                if (score_->style().style_b(Sid::VERTICALLY_STACK_MODIFIERS)) {
                    s += h->get_property(Pid::HARMONY_DO_NOT_STACK_MODIFIERS).to_bool() ? u" stack-degrees=\"no\"" :
                                                                                          u" stack-degrees=\"yes\"";
                }
                xml_.tag_raw(s, xml_kind);

                if (tpc_is_valid(bass_tpc)) {
                    XmlWriter::Attributes bass_attrs = {
                        {"arrangement", score_->style().style_b(Sid::CHORD_BASS_NOTE_STAGGER) ? "diagonal" : "horizontal"}};
                    xml_.start_element("bass", bass_attrs);
                    xml_.tag("bass-step", tpc2step_name(bass_tpc));
                    alter = int(tpc2alter(bass_tpc));
                    if (alter) {
                        xml_.tag("bass-alter", alter);
                    }
                    xml_.end_element();
                }

                StringList l = harmony_xml_degrees(info);
                for (const String &tag : l) {
                    String degree_text;
                    if (xml_kind.starts_with(u"suspended") && tag.starts_with(u"add") && tag.at(3).is_digit() && !kind_text.is_empty() &&
                        kind_text.at(0).is_digit()) {
                        // hack to correct text for suspended chords whose kind text has degree information baked in
                        // (required by some other applications)
                        int tag_degree = tag.mid(3).to_int();
                        String kind_text_extension;
                        for (size_t i = 0; i < kind_text.size() && kind_text.at(i).is_digit(); ++i) {
                            kind_text_extension[i] = kind_text[i];
                        }
                        int kind_extension = kind_text_extension.to_int();
                        if (tag_degree <= kind_extension && (tag_degree & 1) && (kind_extension & 1)) {
                            degree_text = u" text=\"\"";
                        }
                    }
                    xml_.start_element("degree");
                    alter = 0;
                    int idx = 3;
                    if (tag[idx] == '#') {
                        alter = 1;
                        ++idx;
                    } else if (tag[idx] == 'b') {
                        alter = -1;
                        ++idx;
                    }
                    xml_.tag_raw(String(u"degree-value%1").arg(degree_text), tag.mid(idx));
                    xml_.tag("degree-alter", alter); // finale insists on this even if 0
                    if (tag.starts_with(u"add")) {
                        xml_.tag_raw(String(u"degree-type%1").arg(degree_text), "add");
                    } else if (tag.starts_with(u"sub")) {
                        xml_.tag("degree-type", "subtract");
                    } else if (tag.starts_with(u"alt")) {
                        xml_.tag("degree-type", "alter");
                    }
                    xml_.end_element();
                }
            } else {
                if (info->text_name().empty()) {
                    xml_.tag("kind", "none");
                } else {
                    xml_.tag("kind", {{"text", info->text_name()}}, "");
                }

                if (tpc_is_valid(bass_tpc)) {
                    xml_.start_element("bass");
                    xml_.tag("bass-step", tpc2step_name(bass_tpc));
                    alter = int(tpc2alter(bass_tpc));
                    if (alter) {
                        xml_.tag("bass-alter", alter);
                    }
                    xml_.end_element();
                }
            }
        }
        if (offset.is_valid() && offset > Fraction(0, 1)) {
            xml_.tag("offset", calculate_duration_in_divisions(offset, div_));
        } else {
            LOGD("invalid offset");
        }
        if (fd) {
            write_music_xml(fd, xml_);
        }
    } else {
        //
        // export an unrecognized Chord
        // which may contain arbitrary text
        //
        // No supprt for polychords at the moment. Export the first chord from the list.
        const HarmonyInfo *info = h->chords().front();
        const String xml_kind = harmony_xml_kind(info);
        const String text_name = info->text_name();
        switch (h->harmony_type()) {
            case HarmonyType::NASHVILLE: {
                String alter;
                String function_text = harmony_xml_function(info, h);
                if (function_text.empty()) {
                    // we just dump the text as deprecated function
                    xml_.tag("function", text_name);
                    xml_.tag("kind", "none");
                    break;
                } else if (!function_text.at(0).is_digit()) {
                    alter = function_text.at(0);
                    function_text = function_text.at(1);
                }
                xml_.start_element("numeral");
                xml_.tag("numeral-root", function_text);
                if (alter == u"b") {
                    xml_.tag("numeral-alter", "-1");
                } else if (alter == u"#") {
                    xml_.tag("numeral-alter", "1");
                }
                xml_.end_element();
                if (!xml_kind.is_empty()) {
                    String s = u"kind";
                    String kind_text = harmony_xml_text(info);
                    if (!harmony_xml_text(info).empty()) {
                        s += u" text=\"" + kind_text + u"\"";
                    }
                    if (harmony_xml_symbols(info) == "yes") {
                        s += u" use-symbols=\"yes\"";
                    }
                    if (harmony_xml_parens(info) == "yes") {
                        s += u" parentheses-degrees=\"yes\"";
                    }
                    xml_.tag_raw(s, xml_kind);
                } else {
                    // default is major
                    xml_.tag("kind", "major");
                }
            } break;
            case HarmonyType::ROMAN: {
                int alter = 0;
                static const std::wregex roman(L"(b|#)?([ivIV]+)");
                if (text_name.contains(roman)) {
                    StringList matches = text_name.search(roman, {1, 2});
                    xml_.start_element("numeral");
                    if (matches.at(0) == u"b") {
                        alter = -1;
                    } else if (matches.at(0) == u"#") {
                        alter = 1;
                    }
                    const String number_str = matches.at(1);
                    size_t harmony = 1;
                    if (number_str.contains(u"v", CaseSensitivity::CASE_INSENSITIVE)) {
                        if (number_str.starts_with(u"i", CaseSensitivity::CASE_INSENSITIVE)) {
                            harmony = 4;
                        } else {
                            harmony = 4 + number_str.size();
                        }
                    } else {
                        harmony = number_str.size();
                    }
                    xml_.tag("numeral-root", {{"text", number_str}}, harmony);
                    if (alter) {
                        xml_.tag("numeral-alter", alter);
                    }
                    xml_.end_element();
                    // simple check for major or minor
                    xml_.tag("kind", number_str.at(0).is_upper() ? "major" : "minor");
                    // infer inversion from ending digits
                    if (text_name.ends_with(u"64")) {
                        xml_.tag("inversion", 2);
                    } else if (text_name.ends_with(u"6")) {
                        xml_.tag("inversion", 1);
                    }
                    break;
                }
            }
            // fallthrough
            case HarmonyType::STANDARD:
            default: {
                xml_.start_element("root");
                xml_.tag("root-step", {{"text", ""}}, "C");
                xml_.end_element(); // root
                xml_.tag("kind", {{"text", text_name}}, "none");
            } break;
        }
    }
    xml_.end_element();
}

//---------------------------------------------------------
//  canWrite
//---------------------------------------------------------

/**
 Whether a tag corresponding to the given element \p e
 should be included to the exported MusicXML file.
 */

bool ExportMusicXml::can_write(const EngravingItem *e) { return e->visible() || configuration()->export_invisible_elements(); }
} // namespace mu::iex::musicxml
