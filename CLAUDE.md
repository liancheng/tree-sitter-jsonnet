# tree-sitter-jsonnet — working notes

A tree-sitter grammar for [Jsonnet](https://jsonnet.org/ref/spec.html). Author: Cheng Lian.

## Build & test workflow

`grammar.js` is **source**; the parser is the *generated* `src/parser.c` + `src/grammar.json` + `src/node-types.json`. These generated files are committed. After **any** edit to `grammar.js` you MUST regenerate — `tree-sitter test`/`parse`/`fuzz`/`cargo build` all run the *stale* committed parser otherwise (they only recompile `parser.c`, they do NOT re-run codegen).

- **Sign-off (run this to validate any change):** `sh sign-off.sh` — runs `tree-sitter generate && tree-sitter test && tree-sitter fuzz -r --iterations 1000 --edits 5 && cargo test`. Exit 0 = all good.
- Always check `tree-sitter generate` output for **conflicts** (binary/postfix operators are the usual source).
- The scanner (`src/scanner.c`) is an external scanner for text blocks; `tree-sitter fuzz` is the key tool for catching incremental-reparse / serialize-deserialize bugs in it. When editing the scanner, fuzz hard.
- `tree-sitter test --update` regenerates expected corpus trees — always **audit** the result (escape counts, node kinds) rather than trusting it blindly.
- Authoritative language reference is https://jsonnet.org/ref/spec.html — **always check the spec before asserting language behavior**; don't answer from memory. (Note: `curl` may be blocked in-session; use WebFetch, but it only returns a summary — verify fine details by parsing with the `jsonnet` CLI when possible.)

## Conventions

- **Git commit messages:** follow [Semantic Commit Messages](https://gist.github.com/joshbuchea/6f47e86d2510bce28f8e7f42ae84c716) — a concise `<type>: <subject>` line (e.g. `feat:`, `fix:`, `docs:`, `chore:`) with an **empty body**.
- **Corpus format** (match `tree-sitter test --update` output so `--update` never churns the files):
  - Input text follows the closing `===` **immediately** — no blank line between them.
  - After the `---` separator, put **one blank line** before the parse tree. Exception: `:error` / empty-tree cases have no tree, so their `---` is followed by a blank line and then the next `===` (which is the case separator, not a pre-tree blank — leave it).
- **Rule naming:** snake_case, prefer short names — `array_comp` (not `array_comprehension`), `for_spec`/`if_spec` (not `forspec`/`ifspec`).
- Add `field(...)` names to sub-parts of rules (e.g. `object`/`field`, `variable`/`collection`, `left`/`operator`/`right`) so they're queryable.
- Precedence lives in the `PREC` table in `grammar.js`. Postfix (`call`, `field_access`, `index`) = `PREC.highest` (13); unary = 12; binary levels 2–11.

## Grammar implementation status (vs. Jsonnet spec)

Implemented:

- **Literals** — `null`, `true`/`false`, numbers, strings, `self`, `super`, `$`.
- **Arrays** and **array comprehension** `[ e for … if … ]` (`for_spec`/`if_spec`).
- **`local`** bindings, **`function`**, **`if`/`then`/`else`** (`conditional`).
- **Calls** — named args + `tailstrict`.
- **`assert … ;`** (`asserted_expr`).
- **Binary operators** — full precedence (`PREC` table), `prec.left`.
- **Field access** `x.y` and **index/slice** `x[i]`, `x[a:b:c]` — postfix at `PREC.highest`.
- **Unary** `- + ! ~`.
- **Objects** `{ … }` — `field` with `+`/`:`/`::`/`:::`, computed/string/id `field_key`, method sugar `f(x): …`, `object_local`, object-level `assert`.
- **Object comprehension** `{ [k]: v for … }` — visible fields only (`:` or `+:`; hidden `::`/`:::` are rejected by the reference `jsonnet`, which does accept the additive `+:` even though the spec grammar shows only `:`).
- **Object composition** `expr { … }` — `object_apply`, postfix at `PREC.highest`; body is `object` or `object_comp`.
- **`super` field access / index** — `super.x`, `super[e]`; these fall out of `field_access`/`index` taking any `expression`, no dedicated rule.
- **`import` / `importstr` / `importbin`** — `import` rule; operand is `$.string` (a string literal, not an expression, since computed imports are disallowed).
- **`error expr`** — `error` rule; `prec.right`, low precedence so the prefix consumes the whole following expression (`error a + b` = `error (a + b)`).
- **Parenthesized expressions** `( expr )` — `parenthesized` rule (added while building `error`, which exposed that `(1 + 2)` previously misparsed as a zero-width `call`).

Objects need two declared `conflicts` (see the `conflicts` block in `grammar.js`): `[member, object_comp]` (a leading `{ local x = 1, …`) and `[_computed_key, object_comp]` (`{ [expr] : …`) — both because only a later `for` distinguishes a plain object from a comprehension. Object corpus tests live in `test/corpus/object.txt`.

**The grammar now covers the full Jsonnet spec expression grammar.** No known constructs are pending. Future work is refinement (queries for highlighting/tags, better error recovery, edge-case corpus coverage) rather than new syntax.
