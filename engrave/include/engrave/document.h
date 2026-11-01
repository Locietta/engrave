#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <engrave/edit.h>
#include <engrave/audio.h>
#include <engrave/error.h>
#include <engrave/handle.h>
#include <engrave/output.h>
#include <engrave/query.h>

namespace engrave {

// One open score. Every call must come from the thread that created the
// Engine, and the Document must be destroyed before the Engine; destroying it
// closes the score.
struct Document {
    ~Document();
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // 1 after open or create; only committed edits change it. Exports do not,
    // although the writer assigns element ids.
    std::uint64_t revision() const;

    // Problems found while opening or creating the score.
    const std::vector<Diagnostic>& diagnostics() const;

    // Unsupported for a format the engine does not write (see Capabilities).
    // Score selection errors are the same as outline(). Excerpts are standalone
    // scores in every supported format, including MSCX and compressed MusicXML.
    // With capture limits, the output buffer is capped while it grows and
    // intermediate XML buffers and archive compression are charged against the
    // cumulative working allowance. A conservative estimate for those buffers
    // is three times that allowance plus twice max_bytes and 16 KiB, assuming
    // vector capacity does not exceed twice its requested size and an old
    // allocation may coexist with a new one. This excludes layout/repeat lists,
    // archive metadata, encoder internals, and temporary strings used by the
    // writer, so it is not a total-process memory bound. Cancellation is polled
    // in the writer, but cannot interrupt layout or an in-progress deflate call.
    // Bounded or cancellable MIDI is Unsupported because MIDI builds its full
    // playback-event graph before writing. Throws std::runtime_error if the
    // engine's writer fails.
    Result<std::vector<std::byte>> export_score(const ExportOptions& options);

    // Exact legacy measurePositions / segmentPositions data, including expanded
    // repeat event times. Selection errors are the same as outline(); Poisoned
    // after failed recovery. Ensures layout before reading; does not edit revision.
    Result<Positions> positions(PositionKind kind, const ScoreSel& score = {});
    // Bounded capture of the dynamic element/event payload. A counting pass
    // checks the limit before allocating the result; its second pass uses an
    // additional pointer-plus-size_t index for each element. Reserve at most
    // position_capture_index_bound(max_bytes)
    // bytes for that index, plus fixed vector/allocator overhead. This assumes
    // the platform's pointer and size_t have the same alignment and that vector
    // reserve allocates the requested count; it is not a bound on layout, the
    // repeat list, or general process memory. Cancellation is checked at each
    // position and event, but cannot interrupt layout or repeat-list creation.
    Result<Positions> positions(PositionKind kind, const ScoreSel& score, const CaptureOptions& capture);

    // How many pages the selected score has. NotFound, StaleHandle and
    // InvalidArgument as outline() reports them; an excerpt that has never been
    // laid out is laid out first.
    Result<std::int32_t> page_count(const ScoreSel& score = {});

    // Draws pages of the selected score. InvalidArgument for an unknown format, a
    // page range outside the score, a PNG dpi outside 36..1200 or a non-default
    // dpi for PDF/SVG; ResourceLimit for a page the renderer's size limit rejects.
    // Throws std::runtime_error if the engine's renderer fails.
    Result<RenderOutput> render(const RenderOptions& options);

    // The score's playback tracks and the document's default mix, in the same
    // order a render uses.
    Result<AudioTracks> audio_tracks(const ScoreSel& score = {});

    // Renders the selected score's audio and hands the encoded stream to the sink
    // in chunks. Unsupported when the current SoundFont selection is empty, for
    // a sample rate other than the Engine's, and for a preset; NotFound for an
    // unknown SoundFont name or path; InvalidArgument for an invalid SoundFont
    // index, encoder setting, mix value or track; ResourceLimit when the projected
    // output exceeds the 1 GiB cap or a caller-supplied audio byte limit.
    // Throws std::runtime_error if the engine's
    // audio graph or encoder fails.
    Result<AudioRenderResult> render_audio(const AudioOptions& options, const ByteSink& sink);

    // The score's structure: metadata, parts and staves, measures, time, key and
    // tempo changes, and the excerpts. NotFound when nothing has the excerpt id,
    // StaleHandle when the score it names is gone, InvalidArgument when it names
    // something other than a score.
    Result<Outline> outline(const ScoreSel& score = {});

    // The elements that match, in a stable order. NotFound for a property name the
    // catalogue does not know, InvalidArgument for a type name it does not know and for
    // a filter that cannot be answered (an unknown staff or voice, a measure window out
    // of range or too wide, both forms of the window, a tick with a non-positive
    // denominator, a reversed tick range, a cursor
    // past the end, a limit of 0), ResourceLimit when more elements match than one query
    // may report.
    Result<QueryResult> query(const Query& request);

    // The properties of one element.  The default is every property the catalogue
    // lists for its type. NotFound for a property name the catalogue does not know
    // and for a reference nothing answers; InvalidArgument for a property the
    // element's type does not have, and for an OpRef, which only a batch can use.
    Result<std::vector<PropertyRead>> get(const Ref& target, const std::vector<std::string>& props = {});

    // Runs a batch as one transaction. It fails and leaves the document as it was
    // with RevisionConflict when expected_revision is set and is not the current
    // revision, with NotFound, StaleHandle, InvalidArgument or Unsupported for an
    // operation the caller got wrong, and with EditRejected when the engine
    // refused one; Error::op_index names the operation. A committed batch that
    // changed the score advances the revision by one and gives what it created
    // its ids. Throws when the engine failed and the document could not be
    // restored: later read, edit and output calls returning Result report Poisoned.
    //
    // After successful recovery, a failed batch restores the score's content and
    // ids. Its layout is restored exactly too, unless the engine threw or a
    // Snapshot-policy operation had run: the document is then reloaded from a
    // copy and laid out as if saved and reopened, which can place elements
    // differently. Failed recovery throws and poisons the document.
    Result<EditResult> edit(const Batch& batch, std::optional<std::uint64_t> expected_revision = {});

private:
    friend struct Engine;
    friend struct DocumentAccess;
    struct Impl;
    explicit Document(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace engrave
