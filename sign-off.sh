#!/bin/sh

tree-sitter generate &&
	tree-sitter test &&
	uv run --frozen --reinstall-package tree-sitter-jsonnet pytest &&
	cargo test &&
	tree-sitter fuzz -r --iterations 1000 --edits 5
