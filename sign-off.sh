#!/bin/sh

tree-sitter generate &&
	tree-sitter test &&
	tree-sitter fuzz -r --iterations 1000 --edits 5 &&
	cargo test
