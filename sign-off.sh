#!/bin/sh

cargo bin tree-sitter generate &&
	cargo bin tree-sitter test &&
	uv run --frozen --reinstall-package tree-sitter-jsonnet pytest &&
	cargo test &&
	cargo bin tree-sitter fuzz -r --iterations 1000 --edits 5
