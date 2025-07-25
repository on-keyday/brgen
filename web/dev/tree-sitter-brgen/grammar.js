module.exports = grammar({
  name: "brgen",

  extras: ($) => [$._space, $._line, $._comment],

  rules: {
    program: ($) => repeat($._statement),

    _statement: ($) => choice($.int_literal),

    _space: ($) => /[ \t]/,
    _line: ($) => /[\r\n]+/,
    _comment: ($) => /#.*[\r\n]*/,

    // Integer literals
    digit: ($) => /[0-9]+/,
    hex_digit: ($) => /[0-9a-fA-F]+/,
    binary_digit: ($) => /[01]+/,

    int_literal: ($) =>
      choice($.digit, seq("0x", $.hex_digit), seq("0b", $.binary_digit)),
  },
});
