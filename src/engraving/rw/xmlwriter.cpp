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

#include "xmlwriter.h"

#include "types/typesconv.h"

#include "dom/engravingitem.h"
#include "dom/property.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
XmlWriter::XmlWriter(muse::io::IODevice *device) : XmlStreamWriter(device) {}

XmlWriter::~XmlWriter() {}

void XmlWriter::start_element_raw(const String &s) { XmlStreamWriter::start_element_raw(s); }

void XmlWriter::start_element(const AsciiStringView &name, const Attributes &attrs) { XmlStreamWriter::start_element(name, attrs); }

void XmlWriter::start_element(const EngravingObject *se, const Attributes &attrs) { start_element(se->type_name(), se, attrs); }

void XmlWriter::start_element(const AsciiStringView &name, const EngravingObject *se, const Attributes &attrs) {
    XmlStreamWriter::start_element(name, attrs);

    if (record_elements) {
        elements_value.emplace_back(se, name);
    }
}

void XmlWriter::tag(const AsciiStringView &name, const Attributes &attrs) { XmlStreamWriter::element(name, attrs); }

void XmlWriter::tag(const AsciiStringView &name, const Value &body) { XmlStreamWriter::element(name, body); }

void XmlWriter::tag(const AsciiStringView &name, const Value &val, const Value &def) {
    if (val == def) {
        return;
    }
    tag(name, val);
}

void XmlWriter::tag(const AsciiStringView &name, const Attributes &attrs, const Value &body) {
    XmlStreamWriter::element(name, attrs, body);
}

void XmlWriter::tag_raw(const String &element_with_attrs, const Value &body) { XmlStreamWriter::element_raw(element_with_attrs, body); }

//---------------------------------------------------------
//   tag
//---------------------------------------------------------

void XmlWriter::tag_property(Pid id, const PropertyValue &val, const PropertyValue &def) {
    if (val == def) {
        return;
    }

    const char *name = property_name(id);
    if (name == 0) {
        return;
    }

    PType val_type = val.type();
    PType prop_type = property_type(id);
    if (val_type != prop_type) {
        LOGD() << "property value type mismatch, prop: " << name;
    }

    const String writable_val(property_to_string(id, val, /* mscx */ true));
    if (writable_val.is_empty()) {
        //! NOTE The data type is ABSOLUTE, but we write SPATIUM
        //! (the conversion from Absolute to Spatium occurred higher up the stack)
        if (prop_type == PType::ABSOLUTE) {
            prop_type = PType::SPATIUM;
        }

        //! HACK Temporary hack. We used to have some kind of property with property type BOOL,
        //! but the used value type is INT (not just 1 and 0)
        //! TODO: remove if deemed safe
        IF_ASSERT_FAILED(!(prop_type == PType::BOOL && val_type == PType::INT)) { prop_type = PType::INT; }

        tag_property(name, prop_type, val);
    } else {
        tag_property(name, PType::STRING, PropertyValue(writable_val));
    }
}

void XmlWriter::tag_property(const AsciiStringView &name, const PropertyValue &val, const PropertyValue &def) {
    if (val == def) {
        return;
    }

    tag_property(name, val.type(), val);
}

void XmlWriter::tag_property(const AsciiStringView &name, PType type, const PropertyValue &data) {
    switch (type) {
        case PType::UNDEFINED: UNREACHABLE; break;
        // base
        case PType::BOOL: element(name, int(data.value<bool>())); break;
        case PType::INT:
        case PType::SIZE_T: element(name, data.value<int>()); break;
        case PType::REAL: element(name, data.value<double>()); break;
        case PType::STRING: element(name, data.value<String>()); break;
        // geometry
        case PType::POINT: {
            PointF p = data.value<PointF>();
            element(name, {{"x", p.x()}, {"y", p.y()}});
        } break;
        case PType::SIZE: {
            SizeF s = data.value<SizeF>();
            element(name, {{"w", s.width()}, {"h", s.height()}});
        } break;
        case PType::DRAW_PATH:
            UNREACHABLE; //! TODO
            break;
        case PType::SCALE: {
            ScaleF s = data.value<ScaleF>();
            element(name, {{"w", s.width()}, {"h", s.height()}});
        } break;
        case PType::SPATIUM: element(name, data.value<Spatium>().val()); break;
        case PType::ABSOLUTE: element(name, data.value<double>()); break;

        // draw
        case PType::SYMID: {
            element(name, TConv::to_xml(data.value<SymId>()));
        } break;
        case PType::COLOR: {
            Color color(data.value<Color>());
            element(name, {{"r", color.red()}, {"g", color.green()}, {"b", color.blue()}, {"a", color.alpha()}});
        } break;
        case PType::ORNAMENT_STYLE: {
            element(name, TConv::to_xml(data.value<OrnamentStyle>()));
        } break;
        case PType::ORNAMENT_SHOW_ACCIDENTAL: element(name, static_cast<int>(data.value<OrnamentShowAccidental>())); break;
        case PType::GLISS_STYLE: {
            element(name, TConv::to_xml(data.value<GlissandoStyle>()));
        } break;
        case PType::GLISS_TYPE: {
            element(name, TConv::to_xml(data.value<GlissandoType>()));
        } break;
        case PType::ALIGN: {
            element(name, TConv::to_xml(data.value<Align>()));
        } break;
        case PType::ALIGN_H: {
            element(name, TConv::to_xml(data.value<AlignH>()));
        } break;
        case PType::PLACEMENT_V: {
            element(name, TConv::to_xml(data.value<PlacementV>()));
        } break;
        case PType::PLACEMENT_H: {
            element(name, TConv::to_xml(data.value<PlacementH>()));
        } break;
        case PType::TEXT_PLACE: {
            element(name, TConv::to_xml(data.value<TextPlace>()));
        } break;
        case PType::DIRECTION_V: {
            element(name, TConv::to_xml(data.value<DirectionV>()));
        } break;
        case PType::DIRECTION_H: {
            element(name, TConv::to_xml(data.value<DirectionH>()));
        } break;
        case PType::ORIENTATION: {
            element(name, TConv::to_xml(data.value<Orientation>()));
        } break;
        // time
        case PType::FRACTION: {
            tag_fraction(name, data.value<Fraction>());
        } break;
        case PType::LAYOUTBREAK_TYPE: {
            element(name, TConv::to_xml(data.value<LayoutBreakType>()));
        } break;
        case PType::BARLINE_TYPE: {
            element(name, TConv::to_xml(data.value<BarLineType>()));
        } break;
        case PType::NOTEHEAD_TYPE: {
            element(name, TConv::to_xml(data.value<NoteHeadType>()));
        } break;
        case PType::NOTEHEAD_SCHEME: {
            element(name, TConv::to_xml(data.value<NoteHeadScheme>()));
        } break;
        case PType::NOTEHEAD_GROUP: {
            element(name, TConv::to_xml(data.value<NoteHeadGroup>()));
        } break;
        case PType::CLEF_TYPE: {
            element(name, TConv::to_xml(data.value<ClefType>()));
        } break;
        case PType::CLEF_TO_BARLINE_POS: element(name, static_cast<int>(data.value<ClefToBarlinePosition>())); break;
        case PType::DYNAMIC_TYPE: {
            element(name, TConv::to_xml(data.value<DynamicType>()));
        } break;
        case PType::DYNAMIC_SPEED: {
            element(name, TConv::to_xml(data.value<DynamicSpeed>()));
        } break;
        case PType::LINE_TYPE: {
            element(name, TConv::to_xml(data.value<LineType>()));
        } break;
        case PType::HOOK_TYPE: {
            element(name, TConv::to_xml(data.value<HookType>()));
        } break;
        case PType::KEY_MODE: {
            element(name, TConv::to_xml(data.value<KeyMode>()));
        } break;
        case PType::TEXT_STYLE: {
            element(name, TConv::to_xml(data.value<TextStyleType>()));
        } break;
        case PType::SLUR_STYLE_TYPE: element(name, static_cast<int>(data.value<SlurStyleType>())); break;
        case PType::CHANGE_METHOD: {
            element(name, TConv::to_xml(data.value<ChangeMethod>()));
        } break;
        case PType::ACCIDENTAL_ROLE: {
            element(name, TConv::to_xml(data.value<AccidentalRole>()));
        } break;
        case PType::PLAYTECH_TYPE: {
            element(name, TConv::to_xml(data.value<PlayingTechniqueType>()));
        } break;
        case PType::TEMPOCHANGE_TYPE: {
            element(name, TConv::to_xml(data.value<GradualTempoChangeType>()));
        } break;
        case PType::ORNAMENT_INTERVAL: {
            element(name, TConv::to_xml(data.value<OrnamentInterval>()));
        } break;
        case PType::TIE_PLACEMENT: {
            element(name, TConv::to_xml(data.value<TiePlacement>()));
        } break;
        case PType::VOICE_ASSIGNMENT: {
            element(name, TConv::to_xml(data.value<VoiceAssignment>()));
        } break;
        case PType::AUTO_ON_OFF: {
            element(name, TConv::to_xml(data.value<AutoOnOff>()));
        } break;
        case PType::AUTO_CUSTOM_HIDE: {
            element(name, TConv::to_xml(data.value<AutoCustomHide>()));
        } break;
        case PType::INT_VEC: {
            element(name, TConv::to_xml(data.value<std::vector<int>>()));
        } break;
        case PType::PARTIAL_SPANNER_DIRECTION: {
            element(name, TConv::to_xml(data.value<PartialSpannerDirection>()));
        } break;
        case PType::PARENTHESES_MODE: {
            element(name, TConv::to_xml(data.value<ParenthesesMode>()));
        } break;
        case PType::MARKER_TYPE: {
            element(name, TConv::to_xml(data.value<MarkerType>()));
        } break;
        default: {
            UNREACHABLE; //! TODO
        } break;
    }
}

void XmlWriter::tag_point(const AsciiStringView &name, const PointF &p) { tag(name, {{"x", p.x()}, {"y", p.y()}}); }

void XmlWriter::tag_fraction(const AsciiStringView &name, const Fraction &v, const Fraction &def) {
    if (v == def) {
        return;
    }

    element(name, v.to_string());
}

void XmlWriter::write_xml(const String &name, String s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char16_t c = s.at(i).unicode();
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
            s[i] = u'?';
        }
    }

    element_string_raw(name, s);
}

void XmlWriter::comment(const String &text) { XmlStreamWriter::comment(text); }

String XmlWriter::xml_string(const String &s) { return s.to_xml_escaped(); }
} // namespace mu::engraving
