module;

// Keep these declarations in the global module to match the existing static libraries.

#include <engrave/audio.h>
#include <engrave/describe.h>
#include <engrave/document.h>
#include <engrave/edit.h>
#include <engrave/engine.h>
#include <engrave/engrave.h>
#include <engrave/error.h>
#include <engrave/export_warning.h>
#include <engrave/handle.h>
#include <engrave/ops/measures.h>
#include <engrave/ops/notes.h>
#include <engrave/ops/parts.h>
#include <engrave/ops/property.h>
#include <engrave/ops/spanners.h>
#include <engrave/ops/text.h>
#include <engrave/output.h>
#include <engrave/query.h>
#include <engrave/value.h>
#include <engrave/version.h>

export module engrave;

export import std;

export namespace engrave {

using ::engrave::Add;
using ::engrave::AddPart;
using ::engrave::AddSpanner;
using ::engrave::AddStaff;
using ::engrave::AddText;
using ::engrave::AddTie;
using ::engrave::AddTuplet;
using ::engrave::Address;
using ::engrave::AudioFormat;
using ::engrave::AudioMix;
using ::engrave::AudioOptions;
using ::engrave::AudioRenderResult;
using ::engrave::AudioTrackInfo;
using ::engrave::AudioTrackOptions;
using ::engrave::AudioTracks;
using ::engrave::Batch;
using ::engrave::BatchRejection;
using ::engrave::ByteSink;
using ::engrave::Capabilities;
using ::engrave::CaptureOptions;
using ::engrave::Color;
using ::engrave::CreateOptions;
using ::engrave::Diagnostic;
using ::engrave::Document;
using ::engrave::EditResult;
using ::engrave::ElementId;
using ::engrave::ElementInfo;
using ::engrave::Engine;
using ::engrave::EngineOptions;
using ::engrave::EnterNotes;
using ::engrave::Enum;
using ::engrave::Error;
using ::engrave::ErrorCode;
using ::engrave::ExcerptInfo;
using ::engrave::ExportOptions;
using ::engrave::ExportOutput;
using ::engrave::ExportWarning;
using ::engrave::ExportWarningObject;
using ::engrave::Format;
using ::engrave::Fraction;
using ::engrave::GenerateExcerpts;
using ::engrave::InsertClef;
using ::engrave::InsertMeasures;
using ::engrave::LayoutBox;
using ::engrave::MeasureInfo;
using ::engrave::MeasureOps;
using ::engrave::NoteOps;
using ::engrave::Op;
using ::engrave::OpRef;
using ::engrave::OpResult;
using ::engrave::Opaque;
using ::engrave::OpenOptions;
using ::engrave::Outline;
using ::engrave::PageAddress;
using ::engrave::PartInfo;
using ::engrave::PartOps;
using ::engrave::Pitch;
using ::engrave::Point;
using ::engrave::PositionElement;
using ::engrave::PositionEvent;
using ::engrave::PositionKind;
using ::engrave::Positions;
using ::engrave::PropInfo;
using ::engrave::PropertyOps;
using ::engrave::PropertyRead;
using ::engrave::Query;
using ::engrave::QueryResult;
using ::engrave::Ref;
using ::engrave::Remove;
using ::engrave::RemoveMeasures;
using ::engrave::RemovePart;
using ::engrave::RemoveStaff;
using ::engrave::RenderFormat;
using ::engrave::RenderOptions;
using ::engrave::RenderOutput;
using ::engrave::ResetProperty;
using ::engrave::Result;
using ::engrave::RuntimeFontInfo;
using ::engrave::ScoreSel;
using ::engrave::SegmentAddress;
using ::engrave::SetDuration;
using ::engrave::SetKeySig;
using ::engrave::SetLayoutBreak;
using ::engrave::SetPitch;
using ::engrave::SetProperty;
using ::engrave::SetStyle;
using ::engrave::SetTimeSig;
using ::engrave::Severity;
using ::engrave::SignatureInfo;
using ::engrave::Size;
using ::engrave::SoundFontSelection;
using ::engrave::SpannerOps;
using ::engrave::SpannerSegmentAddress;
using ::engrave::StaffInfo;
using ::engrave::StyleInfo;
using ::engrave::SystemAddress;
using ::engrave::TemplateInfo;
using ::engrave::TextOps;
using ::engrave::TypeInfo;
using ::engrave::Unit;
using ::engrave::Value;
using ::engrave::ValueKind;
using ::engrave::default_score_capture_working_bytes;
using ::engrave::element_id_from_string;
using ::engrave::max_input_bytes;
using ::engrave::max_query_measures;
using ::engrave::max_query_results;
using ::engrave::max_render_dpi;
using ::engrave::max_render_pixels;
using ::engrave::min_render_dpi;
using ::engrave::position_capture_index_bound;
using ::engrave::position_payload_bytes;
using ::engrave::to_string;
using ::engrave::version_major;
using ::engrave::version_minor;
using ::engrave::version_patch;
using ::engrave::version_string;

} // namespace engrave

export namespace engrave::detail {

using ::engrave::detail::OpVariant;
using ::engrave::detail::compare_fraction;

} // namespace engrave::detail

export namespace engrave::export_warning_code {

using ::engrave::export_warning_code::automation;
using ::engrave::export_warning_code::custom_chord_list;
using ::engrave::export_warning_code::excerpt;
using ::engrave::export_warning_code::group_bracket;
using ::engrave::export_warning_code::instrument_group_name;
using ::engrave::export_warning_code::invalid_tie;
using ::engrave::export_warning_code::page_lock;
using ::engrave::export_warning_code::part_identity_omitted;
using ::engrave::export_warning_code::shared_part;
using ::engrave::export_warning_code::stave_sharing_label;
using ::engrave::export_warning_code::unsupported_construct;
using ::engrave::export_warning_code::unsupported_style;
using ::engrave::export_warning_code::unsupported_target;

} // namespace engrave::export_warning_code
