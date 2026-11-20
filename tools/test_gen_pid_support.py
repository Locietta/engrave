import unittest
from pathlib import Path

import gen_pid_support as gen


class ParseTypeNamesTests(unittest.TestCase):
    def test_multiple_entries_per_line_and_multiline_rows(self):
        snippet = """
static constexpr auto element_types = std::to_array<ElementItem>({
    {ElementType::NOTE, "Note", {"c", "n", nullptr, 1}, {"c", "N", nullptr, 1}}, {ElementType::REST, "Rest", {"c", "r", nullptr, 1}, {"c", "R", nullptr, 1}},
    {ElementType::MEASURE_NUMBER,
     "MeasureNumber",
     {"engraving", "measure number(s)", nullptr, 1},
     {"engraving", "Measure number(s)", nullptr, 1}},
    {ElementType::EMPTY_TAG, "", {"c", "e", nullptr, 1}, {"c", "E", nullptr, 1}},
});
"""
        self.assertEqual(
            gen.parse_type_names_text(snippet),
            {
                "NOTE": "Note",
                "REST": "Rest",
                "MEASURE_NUMBER": "MeasureNumber",
                "EMPTY_TAG": "",
            },
        )

    def test_ignores_commented_out_rows_and_comments_around_literals(self):
        snippet = """
/* element_types = { {ElementType::FAKE, "Fake"} }; */
static constexpr auto element_types = std::to_array<ElementItem>({
    // {ElementType::NOTE, "WrongLine", {"c", "n", nullptr, 1}, {"c", "N", nullptr, 1}},
    {ElementType::NOTE, /* "NotXml" */ "Note", {"c", "{ElementType::IN_STRING, \\"No\\"}", nullptr, 1}, {"c", "N", nullptr, 1}},
    /* {ElementType::NOTE, "WrongBlock", {"c", "n", nullptr, 1}, {"c", "N", nullptr, 1}}, */
    /* {ElementType::GHOST, "Ghost", {"c", "g", nullptr, 1}, {"c", "G", nullptr, 1}}, */
    {ElementType::CLEF, // xml tag on next line
     "Clef", {"c", "c", nullptr, 1}, {"c", "C", nullptr, 1}},
});
"""
        self.assertEqual(
            gen.parse_type_names_text(snippet),
            {
                "NOTE": "Note",
                "CLEF": "Clef",
            },
        )

    def test_rejects_non_string_xml_tag(self):
        snippet = """
static constexpr auto element_types = std::to_array<ElementItem>({
    {ElementType::NOTE, NOT_A_LITERAL, {"c", "n", nullptr, 1}, {"c", "N", nullptr, 1}},
});
"""
        with self.assertRaises(ValueError):
            gen.parse_type_names_text(snippet)

    def test_repository_typesconv_matches_known_entries(self):
        root = Path(__file__).resolve().parent.parent
        names = gen.parse_type_names(root)
        self.assertEqual(names["NOTE"], "Note")
        self.assertEqual(names["MEASURE_NUMBER"], "MeasureNumber")
        self.assertEqual(names["SLUR_SEGMENT"], "SlurSegment")
        self.assertGreater(len(names), 100)


if __name__ == "__main__":
    unittest.main()
