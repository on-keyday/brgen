// A helper function for sequences separated by a delimiter
const sepBy1 = (sep, rule) => seq(rule, repeat(seq(sep, rule)));
const sepBy = (sep, rule) => optional(sepBy1(sep, rule));

// Precedence levels for expressions, based on your BNF
const PREC = {
  assign: 1,
  ternary: 2,
  range: 3,
  logical_or: 4,
  logical_and: 5,
  compare: 6,
  add: 7,
  mul: 8,
  unary: 9,
  postfix: 10,
  call: 11,
};

module.exports = grammar({
  name: "brgen",

  // Whitespace and comments are handled here. They can appear almost anywhere.
  extras: ($) => [
    /\s/, // Any whitespace character (space, tab, newline)
    $.comment,
  ],

  // Conflicts that might arise and need resolution.
  // Precedence rules for binary operators usually resolve these.
  conflicts: ($) => [
    [$.indent_block],
    [$.enum_member, $.prim],
    [$.enum_member],
  ],

  // Let Tree-sitter know that these rules are variations of a common type.
  supertypes: ($) => [$._statement, $._expression, $._type],

  // Let Tree-sitter know that keywords should be preferred over identifiers.
  word: ($) => $.identifier,

  rules: {
    // Top-level rule: a program is a sequence of statements.
    program: ($) => repeat($._statement),

    // ## Statements ##
    _statement: ($) =>
      choice(
        $.format_definition,
        $.state_definition,
        $.enum_definition,
        $.function_definition,
        $.loop_statement,
        $.return_statement,
        $.break_statement,
        $.continue_statement,
        $.field_definition,
        $.expression_statement
      ),

    expression_statement: ($) => $._expression,

    // Block of indented statements.
    // NOTE: True indentation-sensitivity requires an external scanner.
    // This is a simplified version that captures the structure.
    indent_block: ($) => seq(":", repeat1($._statement)),

    format_definition: ($) =>
      seq("format", field("name", $.identifier), $.indent_block),

    state_definition: ($) =>
      seq("state", field("name", $.identifier), $.indent_block),

    enum_definition: ($) =>
      prec(
        1,
        seq(
          "enum",
          field("name", $.identifier),
          ":",
          field("base_type", optional($.anonymous_field)),
          repeat1($.enum_member)
        )
      ),

    enum_member: ($) =>
      seq(
        field("name", $.identifier),
        optional(
          seq(
            "=",
            choice(
              $._expression,
              seq($._expression, ",", $.string_literal),
              seq($.string_literal, ",", $._expression),
              $.string_literal
            )
          )
        )
      ),

    function_definition: ($) =>
      seq(
        "fn",
        field("name", $.identifier),
        field("parameters", $.parameter_list),
        optional(seq("->", field("return_type", $._type))),
        $.indent_block
      ),

    parameter_list: ($) => seq("(", sepBy(",", $.field_definition), ")"),

    loop_statement: ($) =>
      seq(
        "for",
        // for range loop `for i in expr` or C-style loop `for ; a < b; a++`
        optional(
          choice(
            seq(
              field("variable", $.identifier),
              "in",
              field("range", $._expression)
            ),
            seq(
              optional(field("initializer", $._expression)),
              ";",
              optional(field("condition", $._expression)),
              ";",
              optional(field("increment", $._expression))
            )
          )
        ),
        $.indent_block
      ),

    return_statement: ($) => prec.left(seq("return", optional($._expression))),
    break_statement: ($) => "break",
    continue_statement: ($) => "continue",

    // ## Expressions ##
    _expression: ($) =>
      choice(
        $.unary_expression,
        $.binary_expression,
        $.ternary_expression,
        $.assignment_expression,
        $.postfix_expression,
        $.range_expression,
        $.prim
      ),

    prim: ($) =>
      choice(
        $.identifier,
        $.int_literal,
        $.bool_literal,
        $.string_literal,
        $.regex_literal,
        $.char_literal,
        $.special_literal,
        $.type_literal,
        $.parenthesized_expression,
        $.if_expression,
        $.match_expression
      ),

    parenthesized_expression: ($) => seq("(", $._expression, ")"),

    postfix_expression: ($) =>
      prec(
        PREC.postfix,
        choice($.call_expression, $.member_access, $.index_access)
      ),

    call_expression: ($) =>
      prec(
        PREC.call,
        seq(
          field("function", $._expression),
          field("arguments", $.call_arguments)
        )
      ),

    call_arguments: ($) => seq("(", sepBy(",", $._expression), ")"),

    member_access: ($) =>
      prec(
        PREC.postfix,
        seq(field("object", $._expression), ".", field("member", $.identifier))
      ),

    index_access: ($) =>
      prec(
        PREC.postfix,
        seq(
          field("object", $._expression),
          "[",
          field("index", $._expression),
          "]"
        )
      ),

    unary_expression: ($) =>
      prec.right(
        PREC.unary,
        seq(
          field("operator", choice("-", "!")),
          field("operand", $._expression)
        )
      ),

    binary_expression: ($) =>
      choice(
        ...[
          ["&&", PREC.logical_and],
          ["||", PREC.logical_or],
          ["+", PREC.add],
          ["-", PREC.add],
          ["|", PREC.add],
          ["^", PREC.add],
          ["*", PREC.mul],
          ["/", PREC.mul],
          ["%", PREC.mul],
          ["&", PREC.mul],
          ["<<<", PREC.mul],
          [">>>", PREC.mul],
          ["<<", PREC.mul],
          [">>", PREC.mul],
          ["==", PREC.compare],
          ["!=", PREC.compare],
          ["<", PREC.compare],
          ["<=", PREC.compare],
          [">", PREC.compare],
          [">=", PREC.compare],
        ].map(([operator, precedence]) =>
          prec.left(
            precedence,
            seq(
              field("left", $._expression),
              field("operator", operator),
              field("right", $._expression)
            )
          )
        )
      ),

    range_expression: ($) =>
      prec.left(
        PREC.range,
        seq(
          optional(field("start", $._expression)),
          field("operator", choice("..", "..=")),
          optional(field("end", $._expression))
        )
      ),

    ternary_expression: ($) =>
      prec.right(
        PREC.ternary,
        seq(
          field("condition", $._expression),
          "?",
          field("consequence", $._expression),
          ":",
          field("alternative", $._expression)
        )
      ),

    assignment_expression: ($) =>
      prec.right(
        PREC.assign,
        seq(
          field("left", $._expression),
          field("operator", choice("=", ":=", "::=")),
          field("right", $._expression)
        )
      ),

    if_expression: ($) =>
      prec.right(
        seq(
          "if",
          field("condition", $._expression),
          field("consequence", $.indent_block),
          repeat($.elif_clause),
          optional($.else_clause)
        )
      ),

    elif_clause: ($) =>
      seq(
        "elif",
        field("condition", $._expression),
        field("consequence", $.indent_block)
      ),

    else_clause: ($) => seq("else", field("alternative", $.indent_block)),

    match_expression: ($) =>
      prec(
        1,
        seq(
          "match",
          optional(field("value", $._expression)),
          ":",
          repeat1($.match_branch)
        )
      ),

    match_branch: ($) =>
      seq(
        field("pattern", $._expression),
        field("body", choice(seq("=>", $._statement), $.indent_block))
      ),

    // ## Types and Fields ##
    field_definition: ($) =>
      prec(1, seq(optional(field("name", $.identifier)), $.anonymous_field)),

    anonymous_field: ($) =>
      prec.right(
        1,
        seq(
          ":",
          field("type", $._type),
          optional($.call_arguments) // For default values like `:u8(0)`
        )
      ),

    _type: ($) =>
      choice($.primitive_type, $.array_type, $.function_type, $.custom_type),

    primitive_type: () =>
      choice(
        // Assuming these are common built-in types. Add more as needed.
        "u8",
        "u16",
        "u32",
        "u64",
        "s8",
        "s16",
        "s32",
        "s64",
        "bool"
      ),

    array_type: ($) => seq("[", optional($._expression), "]", $._type),

    function_type: ($) =>
      seq(
        "fn",
        "(",
        sepBy(",", $.function_type_param),
        ")",
        optional(seq("->", $._type))
      ),

    function_type_param: ($) =>
      seq(optional(field("name", $.identifier)), ":", field("type", $._type)),

    custom_type: ($) =>
      prec.left(1, seq($.identifier, repeat(seq(".", $.identifier)))),

    type_literal: ($) => seq("<", $._type, ">"),

    // ## Tokens ##
    comment: ($) => token(seq("#", /.*/)),

    // From your lexer: not_(space_or_punct) implies a broad range of characters.
    // \p{L} matches any Unicode letter, \p{N} any number.
    identifier: ($) => /[_\p{L}][_\p{L}\p{N}]*/,

    special_literal: () => choice("input", "output", "config"), // from BNF & keywords

    bool_literal: () => choice("true", "false"),

    int_literal: () =>
      token(choice(/[0-9]+/, /0x[0-9a-fA-F]+/, /0b[01]+/, /0o[0-7]+/)),

    string_literal: ($) =>
      seq('"', repeat(choice(/[^\\"]+/, $.escape_sequence)), '"'),

    char_literal: ($) => seq("'", choice(/[^\\']/, $.escape_sequence), "'"),

    regex_literal: ($) =>
      seq(
        "/",
        repeat(choice(/[^\\\/]+/, $.escape_sequence)),
        "/",
        optional(/[dgimsuy]+/) // flags
      ),

    escape_sequence: () =>
      token.immediate(/\\([\"'\\\/nrt]|x[0-9a-fA-F]{2}|u[0-9a-fA-F]{4})/),
  },
});
