#!/bin/sh

tree-sitter generate && tree-sitter build && tree-sitter test "$@"
