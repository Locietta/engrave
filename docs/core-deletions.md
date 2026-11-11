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
