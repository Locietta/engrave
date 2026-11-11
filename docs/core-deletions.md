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
