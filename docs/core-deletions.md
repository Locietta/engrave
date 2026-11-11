# Core deletion boundaries

The headless engine has no ShadowNote preview or ActionIcon palette element.
Unknown XML tags, including ActionIcon, are skipped by the native readers.
The public property codec still accepts ActionIconType values; deleting the
palette element does not remove that value type.

ElementType is an internal, contiguous enum. Removing the two element types
renumbers later entries; native score XML identifies elements by name.

Layout always uses the page renderer. The reader and writer retain the native
line/system layoutMode metadata; it no longer selects engine layout behavior.
LayoutConfiguration and page geometry use page semantics, including header/footer
handling and user stretch. Loading no longer discards layoutMode metadata.

The drag/drop virtuals and native reader paste entry points are removed, along
with Paste, EditDuration, ImplodeExplode and the text insertion/paste commands.
The public operations continue using the core note, tuplet, tie and add_text
implementations.

The evidence inventory incorrectly classified TextCursor as desktop-only.
TextBase::create_blocks uses it while parsing formatted text for layout, and
SetProperty/ResetProperty reaches TextBase::undo_change_property, which constructs
ChangeTextProperties for font properties. TextCursor, ChangeTextProperties and its
TextEditUndoableCommand base remain; textedit.cpp is reduced to that command and
the UndoStack payload destructor.

Selection and UndoStack are outside this deletion's scope. Selection::mime_data
still calls EngravingItem::mime_data and the two IWriter selection methods, so
those serialization hooks remain until Selection's MIME layer is removed.
SpannerSegment's MIME override remains with that path. read_mime_data is removed.
UndoStack::undo(EditData*) still inspects TextEditData through ElementEditData;
these payload types and the minimal EditData lookup remain for that protected
boundary. The drag/drop coordinates, modifiers and dropped element are removed.
Grip remains in editdata.h because slur/tie layout and native readers use it.
No Selection, SelectionFilter, InputState, accessibility or UndoStack code is
changed by this step.

Ordinal audit (source review): batch A pins default z stacking to the original
ordinals through default_z. The ShadowNote/ActionIcon rows are removed without
renumbering any remaining legacy ordinal. The regression test checks entries
after both deleted slots. Other numeric ElementType uses found were typed
padding-table/catalogue indexes and transient selection patterns, not persisted
values. Padding entries are assigned by symbolic type, and EID generation does
not use ElementType. Native XML and public queries use type names. Numeric type
codes in factory/MusicXML debug diagnostics can change with the internal enum;
no additional score-format or rendering arithmetic dependency was found.


Accessibility plumbing is removed: Factory flags, EngravingItem flags and the
accessibility hierarchy virtuals have no consumers. The single implementation of
IEngravingContextConfiguration is now the concrete EngravingContextConfiguration;
gui_scaling remains because DebugPaint uses it. Factory name lookup retains its
previous true diagnostics argument to TConv::from_xml. Public describe/query text
(type names, descriptions and format_bars_and_beats) is retained.
This step changes no enumerators, legacy_z rows or persisted numeric values.

Accessibility validation: Windows release engine_tests compiled; the requested API/read/edit/policy/describe/handles/MNX/GP and filtered output suites, plus upstream engraving/MPE suites, executed successfully. Full gate, audio corpus, timing and Linux were not run (requested scope).

UndoableTransaction no longer captures or restores selection/input snapshots or
exposes their unused accessors. Selection and command undo/redo are unchanged;
the existing deselect_all before command replay remains. InputState itself stays:
removing the Score member requires extracting the mixed note-input, deletion and
navigation cluster (cmd.cpp, edit.cpp, noteinput.cpp, edittie.cpp, edittuplet.cpp).
Public EnterNotes -> set_note_rest_at, SetDuration -> change_c_rlen and
AddTuplet -> add_tuplet use explicit targets; AddTie constructs its tie directly.
Those paths are preserved, including transitive calls into mixed editing files.
The inventory understates this compile dependency; the allowed undo-capture-only
scope avoids a broader GUI-command extraction. No enum or persisted value changes.

Undo-capture validation: Windows release engine_tests compiled; all nine requested public suites (output filtered) and upstream engraving/MPE executed successfully. Full gate, audio corpus, timing and Linux not run.

MScore raster/nudge storage and setters, no_gui, svg_printing and the empty
register_ui_types are removed (no consumers). pdf_printing was read only to save
and restore its own value around rendering; paint writes it, but no rendering
branch reads it. Score::printing and paint options remain the actual rendering
controls. Fixed stretch/font/debug/image/template/style-write flags, page gaps
and orientation are constexpr. Warning flags and pedal minimum become constexpr
at their EngineContext-initialized values (true, true, 1), preserving the public
engine lifecycle. The pre-EngineContext zero-initialization was not a public API
state. error, error_is_warning and test_mode stay mutable as requested.
Inventory candidates no_images, save_template_mode and test_write_style_to_score
have reachable reads: native readers/mscloader/BoxLayout, Writer metadata, and
compat::WriteScoreHook respectively; they are constants, not deleted behavior.
No enumerators or persisted numeric defaults change.

MScore validation: Windows release engine_tests compiled; all nine requested public suites (177 tests, output filtered) and upstream engraving/MPE (246 + 24 tests) executed successfully, as for both preceding commits. Full gate, audio corpus, timing and Linux not run.

Selection deletion preparation: removed the uncalled MIME copy writers on
Selection, EngravingItem and SpannerSegment, SelectedSymbol, and both IWriter
selection entry points. Their only filter setter was in write_staff_selection;
WriteContext no longer stores a SelectionFilter or reads Score::selection.
Normal native serialization previously used an absent filter and continues to
accept every item, voice and note index. No element enumerators, legacy_z rows,
property catalogue entries or persisted values change in this preparation.
Selection and SelectionFilter themselves remain; this is not completion of the
requested Selection deletion.

Source audit of the still-pending Selection extraction: all public op files and
score_edit.cpp use explicit targets, not Score::select/selection. Remove calls
delete_item (with an explicit chord for the last note); it does not call
cmd_delete_selection. EnterNotes calls set_note_rest_at and add_note_to_chord;
the selection mutations are in their separate desktop wrappers set_note_rest
and NoteInput::add_note. SetDuration calls change_c_rlen, and AddTuplet calls
add_tuplet/cmd_create_tuplet. Keeping those musical primitives is required.

Remaining selection consumers have not been deleted or classified as unreachable:
- Score ownership, destruction bookkeeping, selection helpers, range scanning,
  split_staff and desktop navigation/selection methods in score.cpp/score.h.
- Element destruction/removal, exclusion from parts, clef/key changes and navigation
  in engravingitem, box, chord, chordrest, clef, keysig, note, segment and measure;
  measure-length repair, excerpt staff cloning, SoundFlag::should_hide,
  MasterScore insertion, and Score::add_figured_bass.
- MasterScore::update, ChangeElement::flip, transaction completion and UndoStack
  undo/redo deselection. Removing only undo snapshots did not remove these uses.
- LayoutContext selection forwarding, SystemHeaderLayout bracket selection,
  ScorePageViewLayout bracket cleanup and TupletLayout number cleanup.
- add_text's harp-diagram branch still reads get_selected_chord_rest rather than
  its destination_element. Other mixed edit.cpp/cmd.cpp selection commands and
  range deletion/full-measure-rest/time deletion remain.
- The editing command families accidental brackets, beam, chord, hairpin,
  enharmonic spelling, measure repeat, note, page/system locks, parentheses,
  rehearsal mark, slash notation, stretch, time signature, split/join measure,
  tie, tuplet, visibility, voice, exchange voices,
  chord-symbol realization, note input, transpose, navigation, flip and regroup.
  Their helper call graphs still need a complete reachability/extraction pass.
- Upstream parts tests exercise selection commands and are compiled into
  engraving_playback_tests; a Selection deletion must preserve or explicitly
  convert these executable editing baselines.

The selected property stays: Pid::SELECTED has ordinary EngravingItem get/set and
false-default handling and an ElementFlag::SELECTED bit, and is exposed in the
property catalogue. No explicit native reader/writer reference to Pid::SELECTED
was found; the common reader enumerates accepted properties rather than accepting
arbitrary property names. It remains runtime data with existing propagation and
rendering behavior. This preparation does not change its semantics.

MIME preparation validation: Windows release engine_tests compiled. The nine
requested public suites executed successfully (179 tests), including Export470.*;
the 12-export standalone-excerpt script also passed. All 17 native corpus inputs
were tried: version_header.mscx exported; 16 returned bounded-writer refusals.
This is an export-attempt inventory, not an executed pre-change comparison.
Upstream engraving executed 246 tests: 245 passed; the wall-clock cancellation
materialization test failed (82.1283 ms checkpoint versus 2.5 ms; 155.686 ms unwind
versus 100 ms). MPE passed 24/24. The broad upstream invocation inadvertently
included two timing assertions despite the requested exclusion; final validation
filters out both CancelsInsideContextMaterialization and
ReleaseBoundsColdOwnersAndFinishesAfterCancellation. No production timing code
or test thresholds were changed. Full gate, audio corpus and Linux were not run.

Accessibility/input-capture leftovers: Score and DummyParent no longer allocate,
initialize, own or expose their unused RootItem objects. RootItem::Kind/kind_ and
the empty init method are removed. The RootItem class/type metadata remains to
keep the existing public property catalogue stable; the engine no longer creates
instances. DummyParent remains the ownership parent of unattached elements.
Score::set_input_state has no callers after undo snapshots were deleted and is
removed. InputState and its input_state accessors stay for the reachable musical
edit primitives documented above. Stale accessibility comments in RootItem,
DummyParent and Selection are removed; Selection::update_state's comment now
describes the selection position it actually updates. No enumerators, legacy_z
rows, property catalogue entries or persisted values change.

Root/input-capture cleanup validation: Windows release engine_tests compiled;
all nine requested public suites executed successfully (179 tests), as did
244 upstream engraving tests (the two timing tests excluded) and 24 MPE tests.
Export470.* and the 12-export standalone-excerpt script passed. Direct 4.70
exports succeeded for nine dedicated compatibility fixtures; ties_hidden and
ties_mismatched were refused, matching their negative-test roles. The 17 general
native-corpus outcomes match the MIME checkpoint (one export, 16 bounded-writer
refusals). This is not a pre-MIME baseline comparison. Formatting checks passed
for all 17 touched C++ files. Full gate, audio corpus and Linux were not run.
Selection/SelectionFilter deletion remains unfinished; no requirement to rewrite
music algorithms was established by this partial extraction.
