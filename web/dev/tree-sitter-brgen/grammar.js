module.exports = grammar({
  name: "brgen",

  extras: ($) => [],

  rules: {
    program: ($) =>
      seq(
        optional($.skip_lines),
        repeat(seq($._statement, optional($.skip_white)))
      ),
    skip_lines: ($) => repeat1(choice($.space, $.comment, $.line)),
    skip_white: ($) => repeat1(choice($.space, $.comment, $.line)),
    skip_space: ($) => repeat1(choice($.space, $.comment)),

    _statement: ($) => choice($.expr),

    space: ($) => /[ \t]+/,
    line: ($) => /[\r\n]+/,
    comment: ($) => /#.*[\r\n]*/,

    expr: ($) => choice($._unary),

    unary_op: ($) => /[-!]/,

    _unary: ($) => choice(seq($.unary_op, $._unary), $.prim),

    prim: ($) => choice($.int_literal),

    // Integer literals
    digit: ($) => /[0-9]+/,
    hex_digit: ($) => /[0-9a-fA-F]+/,
    binary_digit: ($) => /[01]+/,
    octal_digit: ($) => /[0-7]+/,

    int_literal: ($) =>
      choice(
        $.digit,
        seq("0x", $.hex_digit),
        seq("0b", $.binary_digit),
        seq("0o", $.octal_digit)
      ),
  },
});
