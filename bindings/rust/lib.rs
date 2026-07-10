//! This crate provides Jsonnet language support for the [tree-sitter] parsing library.
//!
//! Typically, you will use the [`LANGUAGE`] constant to add this language to a
//! tree-sitter [`Parser`], and then use the parser to parse some code:
//!
//! ```
//! let code = r#"1"#;
//! let mut parser = tree_sitter::Parser::new();
//! let language = tree_sitter_jsonnet::LANGUAGE;
//! parser
//!     .set_language(&language.into())
//!     .expect("Error loading Jsonnet parser");
//! let tree = parser.parse(code, None).unwrap();
//! assert!(!tree.root_node().has_error());
//! ```
//!
//! [`Parser`]: https://docs.rs/tree-sitter/0.26.8/tree_sitter/struct.Parser.html
//! [tree-sitter]: https://tree-sitter.github.io/

use tree_sitter_language::LanguageFn;

extern "C" {
    fn tree_sitter_jsonnet() -> *const ();
}

/// The tree-sitter [`LanguageFn`] for this grammar.
pub const LANGUAGE: LanguageFn = unsafe { LanguageFn::from_raw(tree_sitter_jsonnet) };

/// The content of the [`node-types.json`] file for this grammar.
///
/// [`node-types.json`]: https://tree-sitter.github.io/tree-sitter/using-parsers/6-static-node-types
pub const NODE_TYPES: &str = include_str!("../../src/node-types.json");

#[cfg(with_highlights_query)]
/// The syntax highlighting query for this grammar.
pub const HIGHLIGHTS_QUERY: &str = include_str!("../../queries/highlights.scm");

#[cfg(with_injections_query)]
/// The language injection query for this grammar.
pub const INJECTIONS_QUERY: &str = include_str!("../../queries/injections.scm");

#[cfg(with_locals_query)]
/// The local variable query for this grammar.
pub const LOCALS_QUERY: &str = include_str!("../../queries/locals.scm");

#[cfg(with_tags_query)]
/// The symbol tagging query for this grammar.
pub const TAGS_QUERY: &str = include_str!("../../queries/tags.scm");

#[cfg(test)]
mod tests {
    use tree_sitter::{self as T};

    fn parser() -> T::Parser {
        let mut parser = T::Parser::new();
        parser
            .set_language(&super::LANGUAGE.into())
            .expect("Error loading Jsonnet parser");
        parser
    }

    fn assert_node_eq(
        node: T::Node,
        kind: &str,
        start: impl Into<Option<(usize, usize)>>,
        end: impl Into<Option<(usize, usize)>>,
    ) {
        assert_eq!(node.kind(), kind);

        if let Some((row, column)) = start.into() {
            assert_eq!(node.start_position(), T::Point::new(row, column));
        }

        if let Some((row, column)) = end.into() {
            assert_eq!(node.end_position(), T::Point::new(row, column));
        }
    }

    #[test]
    fn test_parse_number() {
        let tree = parser().parse("1", None).unwrap();
        let root = tree.root_node();
        assert!(!root.has_error());

        assert_eq!(root.kind(), "document");
        assert_eq!(root.child_count(), 1);

        let number = root.child(0).unwrap();
        assert_eq!(number.kind(), "number");
        assert_node_eq(number, "number", (0, 0), (0, 1));
    }

    #[test]
    fn test_parse_text_block() {
        let source = indoc::indoc! {"
            |||
              $
                c
            |||
        "}
        .replace("$", "");

        let tree = parser().parse(source, None).unwrap();
        let root = tree.root_node();
        assert!(!root.has_error());

        assert_eq!(root.kind(), "document");
        assert_eq!(root.child_count(), 1);

        let string = root.child(0).unwrap();
        assert_eq!(string.kind(), "string");

        let text_block = string.child(0).unwrap();

        assert_node_eq(text_block, "text_block", (0, 0), (3, 3));

        assert_eq!(text_block.child(0).unwrap().kind(), "text_block_start");
        assert_eq!(text_block.child(5).unwrap().kind(), "text_block_end");

        assert_node_eq(
            text_block.child(1).unwrap(),
            "text_block_indent",
            (1, 0),
            (1, 2),
        );

        assert_node_eq(
            text_block.child(2).unwrap(),
            "text_block_line_content",
            (1, 2),
            (2, 0),
        );

        assert_node_eq(
            text_block.child(3).unwrap(),
            "text_block_indent",
            (2, 0),
            (2, 2),
        );

        assert_node_eq(
            text_block.child(4).unwrap(),
            "text_block_line_content",
            (2, 2),
            (3, 0),
        );
    }
}
