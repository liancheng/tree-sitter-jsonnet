/**
 * @file Jsonnet grammar for tree-sitter
 * @author Cheng Lian <lian.cs.zju@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const PREC = {
  highest: 12,
  unary: 11,
  multiplicative: 10,
  additive: 9,
  bit_shift: 8,
  comparison: 7,
  equality: 6,
  bit_and: 5,
  bit_xor: 4,
  bit_or: 3,
  and: 2,
  or: 1,
};

export default grammar({
  name: "jsonnet",

  externals: ($) => [
    $.text_block_start,
    $.text_block_blank_line,
    $.text_block_indent,
    $.text_block_line_content,
    $.text_block_end,
  ],

  extras: ($) => [/\s/, $.comment],

  word: ($) => $._id,

  supertypes: ($) => [$.expression, $.string],

  conflicts: ($) => [
    // NOTE: `object_local` (part of `_member`) and `_computed_key` can appear in both `object` and `object_comp`, only
    // a later `for` decides.
    [$._member, $.object_comp],
    [$._computed_key, $.object_comp],
  ],

  rules: {
    document: ($) => $.expression,

    expression: ($) =>
      choice(
        $.array,
        $.array_comp,
        $.asserted_expr,
        $.binary,
        $.boolean,
        $.call,
        $.conditional,
        $.dollar,
        $.error,
        $.field_access,
        $.function,
        $.import,
        $.index,
        $.local,
        $.null,
        $.number,
        $.object,
        $.object_apply,
        $.object_comp,
        $.parenthesized,
        $.self,
        $.super,
        $.unary,
        $.string,
        $.var_ref_id,
      ),

    array: ($) => seq("[", optional(commaSep($.expression)), "]"),

    array_comp: ($) =>
      seq("[", $.expression, optional(","), $.for_spec, repeat(choice($.for_spec, $.if_spec)), "]"),

    for_spec: ($) => seq("for", $.var_id, "in", $.expression),

    if_spec: ($) => seq("if", $.expression),

    object: ($) => seq("{", optional(commaSep($._member)), "}"),

    _member: ($) => choice($.object_local, $.assert, $.field),

    inherit: () => "+",

    visibility: () => choice(":", "::", ":::"),

    field: ($) =>
      choice(
        seq(
          field("key", $.field_key),
          field("inherit", optional($.inherit)),
          field("visibility", $.visibility),
          field("value", $.expression),
        ),
        seq(
          field("key", $.field_key),
          field("parameters", $.params),
          field("visibility", $.visibility),
          field("body", $.expression),
        ),
      ),

    field_key: ($) => choice($._static_key, $._computed_key),

    _static_key: ($) => choice($.field_id, $.string),

    _computed_key: ($) => seq("[", field("expression", $.expression), "]"),

    object_apply: ($) =>
      prec(
        PREC.highest,
        seq(field("target", $.expression), field("object", choice($.object, $.object_comp))),
      ),

    object_local: ($) => seq("local", $.binding),

    object_comp: ($) =>
      seq(
        "{",
        repeat(seq($.object_local, ",")),
        "[",
        field("key", $.expression),
        "]",
        // NOTE: The Jsonnet language spec does not allow `+` here but the Google Jsonnet reference implementation does.
        optional(field("inherit", "+")),
        ":",
        field("value", $.expression),
        repeat(seq(",", $.object_local)),
        optional(","),
        $.for_spec,
        repeat(choice($.for_spec, $.if_spec)),
        "}",
      ),

    asserted_expr: ($) => seq($.assert, ";", $.expression),

    assert: ($) =>
      seq(
        "assert",
        field("condition", $.expression),
        optional(seq(":", field("message", $.expression))),
      ),

    binary: ($) => {
      /** @type {[number, RuleOrLiteral][]} */
      const table = [
        [PREC.multiplicative, $.multiplicative],
        [PREC.additive, $.additive],
        [PREC.bit_shift, $.bit_shift],
        [PREC.comparison, $.comparison],
        [PREC.equality, $.equality],
        [PREC.bit_and, $.bit_and],
        [PREC.bit_xor, $.bit_xor],
        [PREC.bit_or, $.bit_or],
        [PREC.and, $.and],
        [PREC.or, $.or],
      ];

      return choice(
        ...table.map(([precedence, operator]) =>
          prec.left(
            precedence,
            seq(
              field("left", $.expression),
              field("operator", operator),
              field("right", $.expression),
            ),
          ),
        ),
      );
    },

    multiplicative: () => choice("*", "/", "%"),
    additive: () => choice("+", "-"),
    bit_shift: () => choice("<<", ">>"),
    comparison: () => choice("<", "<=", ">", ">=", "in"),
    equality: () => choice("==", "!="),
    bit_and: () => "&",
    bit_xor: () => "^",
    bit_or: () => "|",
    and: () => "&&",
    or: () => "||",

    boolean: () => choice("true", "false"),

    call: ($) => prec(PREC.highest, seq($.expression, $.arguments, optional("tailstrict"))),

    arguments: ($) => seq("(", optional(commaSep($.argument)), ")"),

    argument: ($) => seq(optional(seq(field("name", $.param_ref_id), "=")), $.expression),

    field_access: ($) =>
      prec(PREC.highest, seq(field("object", $.expression), ".", field("field", $.field_ref_id))),

    index: ($) =>
      prec(
        PREC.highest,
        seq(field("object", $.expression), "[", choice(field("index", $.expression), $.slice), "]"),
      ),

    slice: ($) =>
      seq(
        optional(field("start", $.expression)),
        ":",
        optional(field("end", $.expression)),
        optional(seq(":", optional(field("step", $.expression)))),
      ),

    unary: ($) =>
      prec(PREC.unary, seq(field("operator", $.unary_operator), field("operand", $.expression))),

    unary_operator: () => choice("-", "+", "!", "~"),

    // `error expr` — the operand is an arbitrary expression; the prefix
    // extends as far right as possible (e.g. `error a + b` is `error (a + b)`).
    error: ($) => prec.right(seq("error", field("expression", $.expression))),

    parenthesized: ($) => seq("(", field("expression", $.expression), ")"),

    conditional: ($) =>
      prec.right(
        seq(
          "if",
          field("condition", $.expression),
          "then",
          field("consequence", $.expression),
          optional(seq("else", field("alternative", $.expression))),
        ),
      ),

    import_kind: () => choice("import", "importstr", "importbin"),

    // Paths in imports must be single-/double-quoted string literals.
    import: ($) => seq($.import_kind, $.quoted_string),

    function: ($) => seq("function", $.params, $.expression),

    params: ($) => seq("(", optional(commaSep($.param)), ")"),

    param: ($) => seq($.var_id, optional(seq("=", field("default", $.expression)))),

    local: ($) => seq("local", $.bindings, ";", $.expression),

    bindings: ($) => commaSepStrict($.binding),

    binding: ($) =>
      choice(
        seq(field("variable", $.var_id), "=", $.expression),
        seq(field("function", $.var_id), $.params, "=", $.expression),
      ),

    number: () => {
      const binary_literal = /0b[01]+/i;
      const octal_literal = /0o[0-7]+/i;
      const hexical_literal = /0x[0-9a-f]+/i;

      const sign = /[-+]/;
      const decimal_digits = /\d+/;
      const signed_integer = seq(optional(sign), decimal_digits);
      const exponent_part = seq(/e/i, signed_integer);
      const decimal_integer_literal = choice("0", seq(/[1-9]/, optional(decimal_digits)));

      const decimal_literal = choice(
        seq(decimal_integer_literal, ".", optional(decimal_digits), optional(exponent_part)),
        seq(decimal_integer_literal, exponent_part),
        decimal_integer_literal,
      );

      return token(choice(decimal_literal, hexical_literal, binary_literal, octal_literal));
    },

    string: ($) => choice($.quoted_string, $.text_block),

    quoted_string: ($) =>
      choice(
        quotedString($, '"', true),
        quotedString($, "'", true),
        quotedString($, '"', false),
        quotedString($, "'", false),
      ),

    text_block: ($) =>
      seq(
        seq($.text_block_start, token.immediate(/[ \t]*\n/)),
        repeat($.text_block_blank_line),
        seq($.text_block_indent, $.text_block_line_content),
        repeat(
          choice($.text_block_blank_line, seq($.text_block_indent, $.text_block_line_content)),
        ),
        $.text_block_end,
      ),

    dollar: () => "$",
    null: () => "null",
    self: () => "self",
    super: () => "super",

    field_id: ($) => $._id,
    field_ref_id: ($) => $._id,
    param_ref_id: ($) => $._id,
    var_id: ($) => $._id,
    var_ref_id: ($) => $._id,

    _id: () => /[_a-zA-Z][_a-zA-Z0-9]*/,

    comment: () => token(choice(/\/\/.*/, /#.*/, /\/\*[^*]*\*+(?:[^/*][^*]*\*+)*\//)),
  },
});

/** @param {Rule} rule */
function commaSep(rule) {
  return seq(rule, repeat(seq(",", rule)), optional(","));
}

/** @param {Rule} rule */
function commaSepStrict(rule) {
  return seq(rule, repeat(seq(",", rule)));
}

/**
 * @param {GrammarSymbols<string>} $
 * @param {'"' | "'"} quote
 * @param {boolean} verbatim
 */
function quotedString($, quote, verbatim) {
  const start = verbatim ? "@" + quote : quote;
  const end = token.immediate(quote);

  let escape, content;
  if (verbatim) {
    // String `""""` (4 double-quotes) parses correctly as:
    //
    //  (quoted_string
    //    (string_start)      -> "
    //    (escape_sequence)   -> ""
    //    (escape_end))       -> "
    //
    // This is because `""` is longer than `"` and takes a higher precedence.
    escape = token.immediate(quote + quote);
    content = token.immediate(prec(1, new RegExp(`[^${quote}]+`)));
  } else {
    const simpleEsc = /\\["'\\/bfnrt]/;
    const codepointEsc = /\\u[0-9a-fA-F]{4}/;
    escape = token.immediate(choice(simpleEsc, codepointEsc));
    // Stops content at the quote and at the backslash starting an escape.
    // The `prec` keeps string content winning over the `//`/`/*` comment
    // tokens when a string body starts with a slash (e.g. `"//path"`).
    content = token.immediate(prec(1, new RegExp(`[^${quote}\\\\]+`)));
  }

  return seq(
    alias(start, $.string_start),
    repeat(choice(alias(escape, $.escape_sequence), alias(content, $.string_content))),
    alias(end, $.string_end),
  );
}
