#!/bin/sh

cargo test &&
	tree-sitter test &&
	tree-sitter fuzz -r --iterations 1000 --edits 5
