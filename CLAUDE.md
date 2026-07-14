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

- **Corpus format:** input text follows the closing `===` immediately, with **no blank line** between them.
- **Rule naming:** snake_case, prefer short names — `array_comp` (not `array_comprehension`), `for_spec`/`if_spec` (not `forspec`/`ifspec`).
- Add `field(...)` names to sub-parts of rules (e.g. `object`/`field`, `variable`/`collection`, `left`/`operator`/`right`) so they're queryable.
- Precedence lives in the `PREC` table in `grammar.js`. Postfix (`call`, `field_access`, `index`) = `PREC.highest` (13); unary = 12; binary levels 2–11.

## Grammar implementation status (vs. Jsonnet spec)

Implemented: literals (null/bool/number/string/self/super/$), arrays, **array comprehension**, `local`, `function`, `if/then/else`, calls (named args + `tailstrict`), `assert;`, **binary operators** (full precedence), **field access** `x.y`, **index/slice** `x[i]`, `x[a:b:c]`, **unary** `- + ! ~`.

**Still pending (rough priority order):**
1. **Objects** `{ … }` — the biggest gap; needs `member`/`field` (`:`/`::`/`:::`, `+:` variants)/`objlocal`/object-level `assert`. Unlocks object composition `e { … }` and object comprehension.
2. `super` field access / index (`super.x`, `super[e]`) — bare `super` parses, access forms don't.
3. `import` / `importstr` / `importbin`.
4. `error expr`.
