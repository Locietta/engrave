"""Independent engine validation helpers extracted from the host tests."""

from __future__ import annotations

KNOWN_LOSSES: dict[str, dict[str, tuple[str, str]]] = {
    "bend.gp": {
        "musicxml": (
            "f386eae23b9c8cb7",
            "The same GP grace-after notes become grace-before on the following chord; the corrected 5.00 parser changes the digest representation.",
        ),
    },
    "drums_import.musicxml": {
        "mscz_text": (
            "77d7cc79933f0666",
            "The same title size 24 is synthesized on reopen as in v1 (text digest 3a759cb7769875f9); this digest pins its v2 XML location and value.",
        ),
    },
    "custom_part_name.mscx": {
        "musicxml": ("05dd75d215fedcc1", "invisible fff dynamic is not exported"),
    },
    "hairpins_and_repeats.mscx": {
        "musicxml": ("05dd75d215fedcc1", "invisible fff dynamic is not exported"),
    },
    "layout_elements.mscx": {
        "musicxml": (
            "4a1004c05107bbb7",
            "HairPin beginText exported as dynamic mf; harmony moved to staff 1",
        ),
        "idempotence": (
            "aeab1d528f30d872",
            "unrendered MusicXML harmony dropped on second export",
        ),
    },
    "linked_parts.mscx": {
        "musicxml": (
            "fb3d4f6354501cb7",
            "The same grace conversion and backward cross-staff tie occur in historical 4.7.6 A/C files; the corrected 5.00 parser describes them differently.",
        ),
        "idempotence": (
            "efa1ff9698e4b076",
            "the tie re-attached to moved grace note is dropped on second import",
        ),
    },
    "linked_parts.mscx:excerpt:0": {
        "mscz_text": (
            "ed93cce937e37df7",
            "Standalone excerpt reopen loses only the root name Flute; the historical v1 text digest 77d073cac18ac443 also covered lost style and links, which v2 preserves.",
        ),
        "musicxml": (
            "fb3d4f6354501cb7",
            "Same grace conversion and backward cross-staff tie as the linked master; parser representation changed.",
        ),
        "idempotence": (
            "efa1ff9698e4b076",
            "the tie re-attached to moved grace note is dropped on second import",
        ),
    },
    "slurs.mscz": {
        "musicxml": (
            "02ce304f5126a1fd",
            "grace16after slurs: grace-after notes converted to grace-before in MusicXML",
        ),
    },
    "ghost_note.mscx": {
        "musicxml": (
            "7872bae4c4cf03e9",
            "linked tab staff gains an explicit 4/4 time signature after MusicXML import",
        ),
    },
    "tab_grace_prebend.mscx": {
        "musicxml": (
            "7872bae4c4cf03e9",
            "linked tab staff gains an explicit 4/4 time signature after MusicXML import",
        ),
    },
}
