from textwrap import dedent
from unittest import TestCase

import tree_sitter_jsonnet

from tree_sitter import Language, Node, Parser, Point, Tree


def just[U](v: U | None) -> U:
    assert v is not None
    return v


class TestLanguage(TestCase):
    parser = Parser(Language(tree_sitter_jsonnet.language()))

    def parse(self, source: str) -> Tree:
        return self.parser.parse(dedent(source).encode())

    def checkNode(self, node: Node, name: str, start_point: Point, end_point: Point):
        self.assertEqual(node.grammar_name, name)
        self.assertEqual(node.start_point, start_point)
        self.assertEqual(node.end_point, end_point)

    def test_text_block(self):
        tree = self.parse(
            """\
                |||
                    text
            |||
            """
        )

        text_block = just(tree.root_node.child(0))
        self.assertEqual(text_block.grammar_name, "text_block")

        self.checkNode(
            just(text_block.child(0)),
            "text_block_start",
            Point(0, 4),
            Point(0, 7),
        )

        self.checkNode(
            just(text_block.child(1)),
            "text_block_indent",
            Point(1, 0),
            Point(1, 8),
        )

        self.checkNode(
            just(text_block.child(2)),
            "text_block_line_content",
            Point(1, 8),
            Point(2, 0),
        )

        self.checkNode(
            just(text_block.child(3)),
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

        text_block = just(tree.root_node.child(0))

        self.checkNode(
            just(text_block.child(0)),
            "text_block_start",
            Point(0, 4),
            Point(0, 8),
        )
