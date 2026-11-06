# MNX JSON schema (test data)

`mnx-schema.json` is the W3C MNX schema (JSON Schema draft 2020-12) that the
pinned mnxdom package embeds for its own validator. It is copied unchanged
from mnxdom `45bf46221c00411e968c851ff9a1be4e202af5c2`,
`third_party/w3c-mnx/mnx-schema.json`, which records W3C MNX commit
`46fbe9393067221ee83ef08de209cb3c4edf0d8d`
(https://github.com/w3c-cg/mnx). SHA-256:
`71cce118e2bb4d17d42790db2574e84e2b7ba0f0be020c866f71b8f89b638fb8`.

mnxdom's C++ validator implements draft 7, so it does not enforce every
2020-12 keyword. The host engine gate therefore also validates the files
`engrave_mnx_import_test --mnx-out=DIR` exports against this schema with a
draft 2020-12 validator. Update this copy whenever the mnxdom pin moves.

The schema is published under the W3C Software and Document License.
