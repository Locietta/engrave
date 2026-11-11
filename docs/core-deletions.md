# Core deletion boundaries

The headless engine has no ShadowNote preview or ActionIcon palette element.
Unknown XML tags, including ActionIcon, are skipped by the native readers.
The public property codec still accepts ActionIconType values; deleting the
palette element does not remove that value type.

ElementType is an internal, contiguous enum. Removing the two element types
renumbers later entries; native score XML identifies elements by name.
