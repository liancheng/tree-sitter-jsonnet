#!/bin/sh

tree-sitter generate &&
	tree-sitter test &&
	tree-sitter fuzz -r --iterations 1000 --edits 5 &&
	cargo test &&
	uv run --frozen --reinstall-package tree-sitter-jsonnet pytest
