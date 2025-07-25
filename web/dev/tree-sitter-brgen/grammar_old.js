module.exports = grammar({
  name: "brgen",

  extras: ($) => [],

  conflicts: ($) => [[$.expr]],

  rules: {
    program: ($) =>
      seq(
        optional($._skip_lines),
        repeat(seq($._statement, optional($._skip_white)))
      ),
    _skip_lines: ($) => repeat1(choice($._space, $._comment, $._line)),
    _skip_white: ($) => repeat1(choice($._space, $._comment, $._line)),
    _skip_space: ($) => repeat1(choice($._space, $._comment)),

    _statement: ($) => choice($.expr),

    _space: ($) => /[ \t]+/,
    _line: ($) => /[\r\n]+/,
    _comment: ($) => /#.*[\r\n]*/,

    identifier: ($) => /[a-zA-Z_][a-zA-Z0-9_]*/,

    mul_op: ($) => choice("*", "/", "%", "&", "<<<", ">>>", "<<", ">>"),
    add_op: ($) => choice("+", "-", "|", "^"),
    cmp_op: ($) => choice("==", "!=", "<", ">", "<=", ">="),
    logical_and_op: ($) => "&&",
    logical_or_op: ($) => "||",

    expr: ($) =>
      choice(
        prec.left(
          7,
          seq($.expr, $._skip_space, $.logical_or_op, $._skip_white, $.expr)
        ),
        prec.left(
          8,
          seq($.expr, $._skip_space, $.logical_and_op, $._skip_white, $.expr)
        ),
        prec.left(
          9,
          seq($.expr, $._skip_space, $.cmp_op, $._skip_white, $.expr)
        ),
        prec.left(
          10,
          seq($.expr, $._skip_space, $.add_op, $._skip_white, $.expr)
        ),
        prec.left(
          20,
          seq($.expr, $._skip_space, $.mul_op, $._skip_white, $.expr)
        ),
        prec.left(30, $._unary)
      ),

    unary_op: ($) => /[-!]/,

    _unary: ($) => choice(seq($.unary_op, $._unary), $.postfix),

    postfix: ($) => seq($.prim, repeat(choice($.member_access, $.call))),

    member_access: ($) => seq(".", $.identifier),
    call: ($) => seq("(", optional($.args), ")"),
    args: ($) => seq($.expr, repeat(seq(",", $.expr))),

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
