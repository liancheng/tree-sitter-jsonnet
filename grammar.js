/**
 * @file Jsonnet grammar for tree-sitter
 * @author Cheng Lian <lian.cs.zju@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const PREC = {
  highest: 13,
  unary: 12,
  multiplicative: 11,
  additive: 10,
  bit_shift: 9,
  comparison: 8,
  equality: 7,
  bit_and: 6,
  bit_xor: 5,
  bit_or: 4,
  and: 3,
  or: 2,
  object_member: 1,
};

export default grammar({
  name: "jsonnet",

  externals: $ => [
    $.text_block_start,
    $.text_block_blank_line,
    $.text_block_indent,
    $.text_block_line_content,
    $.text_block_end,
  ],

  extras: $ => [
    /\s/,
    $.comment,
  ],

  word: $ => $._id,

  supertypes: $ => [$.expression],

  rules: {
    document: $ => $.expression,

    expression: $ => choice(
      $.array,
      $.asserted_expression,
      $.binary,
      $.boolean,
      $.call,
      $.conditional,
      $.dollar,
      $.field_access,
      $.function,
      $.index,
      $.local,
      $.null,
      $.number,
      $.self,
      $.super,
      $.unary,
      $.string,
      $.var_ref_id,
    ),

    array: $ => seq("[", optional(commaSep($.expression)), "]"),

    asserted_expression: $ => seq($.assert, ";", $.expression),

    assert: $ => seq(
      "assert",
      field("condition", $.expression),
      optional(
        seq(":", field("message", $.expression))
      )
    ),

    binary: $ => {
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
            )
          )
        )
      )
    },

    multiplicative: _ => choice("*", "/", "%"),
    additive: _ => choice("+", "-"),
    bit_shift: _ => choice("<<", ">>"),
    comparison: _ => choice("<", "<=", ">", ">=", "in"),
    equality: _ => choice("==", "!="),
    bit_and: _ => "&",
    bit_xor: _ => "^",
    bit_or: _ => "|",
    and: _ => "&&",
    or: _ => "||",

    boolean: _ => choice("true", "false"),

    call: $ => prec(
      PREC.highest,
      seq(
        field("callee", $.expression),
        $.arguments,
        optional("tailstrict"),
      )
    ),

    arguments: $ => seq("(", optional(commaSep($.argument)), ")"),

    argument: $ => seq(
      optional(seq(field("param", $.param_ref_id), "=")),
      field('value', $.expression),
    ),

    field_access: $ => prec(
      PREC.highest,
      seq(
        field("object", $.expression),
        ".",
        field("field", $.field_ref_id),
      )
    ),

    index: $ => prec(
      PREC.highest,
      seq(
        field("object", $.expression),
        "[",
        choice(
          field("index", $.expression),
          $.slice,
        ),
        "]",
      )
    ),

    slice: $ => seq(
      optional(field("start", $.expression)),
      ":",
      optional(field("end", $.expression)),
      optional(seq(":", optional(field("step", $.expression)))),
    ),

    unary: $ => prec(
      PREC.unary,
      seq(
        field("operator", $.unary_operator),
        field("operand", $.expression),
      )
    ),

    unary_operator: _ => choice("-", "+", "!", "~"),

    conditional: $ => prec.right(
      seq(
        "if", field("condition", $.expression),
        "then", field("consequence", $.expression),
        optional(
          seq("else", field("alternative", $.expression))
        )
      )
    ),

    function: $ => seq(
      "function",
      field("params", $.params),
      field("body", $.expression)
    ),

    params: $ => seq("(", optional(commaSep($.param)), ")"),

    param: $ => seq(
      $.var_id,
      optional(seq("=", field("default", $.expression)))
    ),

    local: $ => seq(
      "local",
      field("bindings", commaSepStrict($.binding)),
      ";",
      field("body", $.expression),
    ),

    binding: $ => choice(
      seq(field("function", $.var_id), field("params", $.params), "=", field("body", $.expression)),
      seq(field("variable", $.var_id), "=", field("value", $.expression)),
    ),

    number: _ => {
      const binary_literal = /0b[01]+/i;
      const octal_literal = /0o[0-7]+/i;
      const hexical_literal = /0x[0-9a-f]+/i;

      const sign = /[-+]/;
      const decimal_digits = /\d+/;
      const signed_integer = seq(sign, decimal_digits);
      const exponent_part = seq(/e/i, signed_integer);
      const decimal_integer_literal = seq(
        optional(sign),
        choice("0", seq(/[1-9]/, optional(decimal_digits))),
      );

      const decimal_literal = choice(
        seq(
          decimal_integer_literal,
          ".",
          optional(decimal_digits),
          optional(exponent_part),
        ),
        seq(decimal_integer_literal, exponent_part),
        decimal_integer_literal,
      );

      return token(
        choice(
          decimal_literal,
          hexical_literal,
          binary_literal,
          octal_literal,
        )
      )
    },

    string: $ => choice(
      $.quoted_string,
      $.text_block,
    ),

    quoted_string: $ => choice(
      quotedString($, '"', true),
      quotedString($, "'", true),
      quotedString($, '"', false),
      quotedString($, "'", false),
    ),

    text_block: $ => seq(
      $.text_block_start,
      repeat($.text_block_blank_line),
      seq($.text_block_indent, $.text_block_line_content),
      repeat(
        choice(
          $.text_block_blank_line,
          seq($.text_block_indent, $.text_block_line_content),
        )
      ),
      $.text_block_end,
    ),

    dollar: _ => "$",
    null: _ => "null",
    self: _ => "self",
    super: _ => "super",

    field_id: $ => $._id,
    field_ref_id: $ => $._id,
    param_ref_id: $ => $._id,
    var_id: $ => $._id,
    var_ref_id: $ => $._id,

    _id: _ => /[_a-zA-Z][_a-zA-Z0-9]*/,

    comment: _ => token(
      choice(
        /\/\/.*/,
        /#.*/,
        /\/\*[^*]*\*+(?:[^/*][^*]*\*+\/)*/,
      )
    ),
  }
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
  const start = verbatim ? '@' + quote : quote;
  const end = token.immediate(quote);

  let escape, content;
  if (verbatim) {
    // String `""""` (4 double-quotes) parses correctly as:
    //
    //  (string
    //    (string_start)      -> "
    //    (escape_sequence)   -> ""
    //    (escape_end))       -> "
    //
    // This is because `""` is longer than `"` and takes a higher precedence.
    escape = token.immediate(quote + quote);
    content = token.immediate(new RegExp(`[^${quote}]+`));
  } else {
    const simpleEsc = /\\["'\\/bfnrt]/;
    const codepointEsc = /\\u[0-9a-fA-F]{4}/;
    escape = token.immediate(choice(simpleEsc, codepointEsc));
    // Stops content at the quote and at the backslash starting an escape.
    content = token.immediate(new RegExp(`[^${quote}\\\\]+`));
  }

  return seq(
    alias(start, $.string_start),
    repeat(choice(
      alias(escape, $.escape_sequence),
      alias(content, $.string_content),
    )),
    alias(end, $.string_end),
  );
}
