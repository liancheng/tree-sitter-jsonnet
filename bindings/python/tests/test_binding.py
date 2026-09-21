from textwrap import dedent
from unittest import TestCase

import tree_sitter_jsonnet

from tree_sitter import Language, Node, Parser, Point, Tree


def nth_child(node: Node, n: int) -> Node:
    child = node.child(n)
    assert child is not None
    return child


class TestLanguage(TestCase):
    parser = Parser(Language(tree_sitter_jsonnet.language()))

    def parse(self, source: str) -> Tree:
        return self.parser.parse(dedent(source).encode())

    def checkNode(self, node: Node, name: str, start_point: Point, end_point: Point):
        self.assertEqual(node.grammar_name, name)
        self.assertEqual(node.start_point, start_point)
        self.assertEqual(node.end_point, end_point)

    def test_quoted_string(self):
        tree = self.parse(r'@"\n"')

        quoted_string = nth_child(tree.root_node, 0)
        self.assertEqual(quoted_string.grammar_name, "quoted_string")

        self.checkNode(
            nth_child(quoted_string, 0),
            "string_start",
            Point(0, 0),
            Point(0, 2),
        )

        self.checkNode(
            nth_child(quoted_string, 1),
            "string_content",
            Point(0, 2),
            Point(0, 4),
        )

        self.checkNode(
            nth_child(quoted_string, 2),
            "string_end",
            Point(0, 4),
            Point(0, 5),
        )

    def test_text_block(self):
        tree = self.parse(
            """\
                |||
                    text
            |||
            """
        )

        text_block = nth_child(tree.root_node, 0)
        self.assertEqual(text_block.grammar_name, "text_block")

        self.checkNode(
            nth_child(text_block, 0),
            "text_block_start",
            Point(0, 4),
            Point(0, 7),
        )

        self.checkNode(
            nth_child(text_block, 1),
            "text_block_indent",
            Point(1, 0),
            Point(1, 8),
        )

        self.checkNode(
            nth_child(text_block, 2),
            "text_block_line_content",
            Point(1, 8),
            Point(2, 0),
        )

        self.checkNode(
            nth_child(text_block, 3),
            "text_block_end",
            Point(2, 0),
            Point(2, 3),
        )

    def test_text_block_strip_last_newline(self):
        tree = self.parse(
            """\
                |||-
                    text
            |||
            """
        )

        text_block = nth_child(tree.root_node, 0)

        self.checkNode(
            nth_child(text_block, 0),
            "text_block_start",
            Point(0, 4),
            Point(0, 8),
        )

    def test_text_block_closed_by_a_space_indented_fence(self):
        tree = self.parse(
            """\
            |||
            \t    text
              |||
            """
        )

        text_block = nth_child(tree.root_node, 0)
        self.assertEqual(text_block.grammar_name, "text_block")

        self.checkNode(
            nth_child(text_block, 0),
            "text_block_start",
            Point(0, 0),
            Point(0, 3),
        )

        self.checkNode(
            nth_child(text_block, 1),
            "text_block_indent",
            Point(1, 0),
            Point(1, 5),
        )

        self.checkNode(
            nth_child(text_block, 2),
            "text_block_line_content",
            Point(1, 5),
            Point(2, 0),
        )

        # Starts past the two spaces indenting the fence, not at column 0.
        self.checkNode(
            nth_child(text_block, 3),
            "text_block_end",
            Point(2, 2),
            Point(2, 5),
        )

    def test_text_block_closed_by_a_tab_indented_fence(self):
        tree = self.parse(
            """\
            |||
                text
            \t|||
            """
        )

        text_block = nth_child(tree.root_node, 0)

        self.checkNode(
            nth_child(text_block, 3),
            "text_block_end",
            Point(2, 1),
            Point(2, 4),
        )
