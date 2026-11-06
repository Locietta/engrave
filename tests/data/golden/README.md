# Current golden contract

Each platform directory contains one `index.json` mapping fixture and artifact
names to SHA-256 values. It includes MNX imports, fixed edit scenarios and
excerpts. The comparator verifies the complete indexed inventory before comparing
fresh outputs. Missing and extra artifacts fail, as do unapproved differences.
`allow.json` holds explicit reviewed differences pinned by platform and hash.

Expected bytes come from previously reviewed indexes and exact oracle/allow-list
pins. Capturing a new output is not approval to change the baseline. Semantic
content, audio evidence, positions, EID relationships, rendered-page statistics
and warning checks remain independent validation lanes. Historical provenance in
page references and compatibility corpus manifests retains its original paths
and commits.

The host exports standalone MSCX; the engine gate also checks MSCX extracted
from MSCZ. The reviewed archive representation is an exact `archive MSCX`
allow-list entry where it differs from the standalone indexed artifact.
