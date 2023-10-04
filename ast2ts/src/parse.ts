
import * as ast from "./ast";

interface parsedNode {
    node_type: string,
    loc: ast.Loc,
    body: any,
}

interface parsedScope {
    prev: number,
    next: number,
    branch: number,
    ident: number[]
}


const parseSchema = {
    "node": [
      {
        "node_type": "node",
        "one_of": [
          "program",
          "expr",
          "binary",
          "unary",
          "cond",
          "ident",
          "call",
          "if",
          "member_access",
          "paren",
          "index",
          "match",
          "range",
          "tmp_var",
          "block_expr",
          "import",
          "literal",
          "int_literal",
          "bool_literal",
          "str_literal",
          "input",
          "output",
          "config",
          "stmt",
          "loop",
          "indent_scope",
          "match_branch",
          "return",
          "break",
          "continue",
          "assert",
          "implicit_yield",
          "member",
          "field",
          "format",
          "function",
          "type",
          "int_type",
          "ident_type",
          "int_literal_type",
          "str_literal_type",
          "void_type",
          "bool_type",
          "array_type",
          "function_type",
          "struct_type",
          "union_type"
        ]
      },
      {
        "node_type": "program",
        "loc": "loc",
        "body": {
          "node_type": "program",
          "loc": "loc",
          "struct_type": "uintptr",
          "elements": "array<uintptr>",
          "global_scope": "uintptr"
        }
      },
      {
        "node_type": "expr",
        "one_of": [
          "binary",
          "unary",
          "cond",
          "ident",
          "call",
          "if",
          "member_access",
          "paren",
          "index",
          "match",
          "range",
          "tmp_var",
          "block_expr",
          "import",
          "literal",
          "int_literal",
          "bool_literal",
          "str_literal",
          "input",
          "output",
          "config"
        ]
      },
      {
        "node_type": "binary",
        "loc": "loc",
        "body": {
          "node_type": "binary",
          "loc": "loc",
          "expr_type": "uintptr",
          "op": "binary_op",
          "left": "uintptr",
          "right": "uintptr"
        }
      },
      {
        "node_type": "unary",
        "loc": "loc",
        "body": {
          "node_type": "unary",
          "loc": "loc",
          "expr_type": "uintptr",
          "op": "unary_op",
          "expr": "uintptr"
        }
      },
      {
        "node_type": "cond",
        "loc": "loc",
        "body": {
          "node_type": "cond",
          "loc": "loc",
          "expr_type": "uintptr",
          "cond": "uintptr",
          "then": "uintptr",
          "els_loc": "loc",
          "els": "uintptr"
        }
      },
      {
        "node_type": "ident",
        "loc": "loc",
        "body": {
          "node_type": "ident",
          "loc": "loc",
          "expr_type": "uintptr",
          "ident": "string",
          "usage": "ident_usage",
          "base": "uintptr",
          "scope": "uintptr"
        }
      },
      {
        "node_type": "call",
        "loc": "loc",
        "body": {
          "node_type": "call",
          "loc": "loc",
          "expr_type": "uintptr",
          "callee": "uintptr",
          "raw_arguments": "uintptr",
          "arguments": "array<uintptr>",
          "end_loc": "loc"
        }
      },
      {
        "node_type": "if",
        "loc": "loc",
        "body": {
          "node_type": "if",
          "loc": "loc",
          "expr_type": "uintptr",
          "cond": "uintptr",
          "then": "uintptr",
          "els": "uintptr"
        }
      },
      {
        "node_type": "member_access",
        "loc": "loc",
        "body": {
          "node_type": "member_access",
          "loc": "loc",
          "expr_type": "uintptr",
          "target": "uintptr",
          "member": "string",
          "member_loc": "loc"
        }
      },
      {
        "node_type": "paren",
        "loc": "loc",
        "body": {
          "node_type": "paren",
          "loc": "loc",
          "expr_type": "uintptr",
          "expr": "uintptr",
          "end_loc": "loc"
        }
      },
      {
        "node_type": "index",
        "loc": "loc",
        "body": {
          "node_type": "index",
          "loc": "loc",
          "expr_type": "uintptr",
          "expr": "uintptr",
          "index": "uintptr",
          "end_loc": "loc"
        }
      },
      {
        "node_type": "match",
        "loc": "loc",
        "body": {
          "node_type": "match",
          "loc": "loc",
          "expr_type": "uintptr",
          "cond": "uintptr",
          "branch": "array<uintptr>",
          "scope": "uintptr"
        }
      },
      {
        "node_type": "range",
        "loc": "loc",
        "body": {
          "node_type": "range",
          "loc": "loc",
          "expr_type": "uintptr",
          "op": "binary_op",
          "start": "uintptr",
          "end": "uintptr"
        }
      },
      {
        "node_type": "tmp_var",
        "loc": "loc",
        "body": {
          "node_type": "tmp_var",
          "loc": "loc",
          "expr_type": "uintptr",
          "tmp_var": "uint"
        }
      },
      {
        "node_type": "block_expr",
        "loc": "loc",
        "body": {
          "node_type": "block_expr",
          "loc": "loc",
          "expr_type": "uintptr",
          "calls": "array<uintptr>",
          "expr": "uintptr"
        }
      },
      {
        "node_type": "import",
        "loc": "loc",
        "body": {
          "node_type": "import",
          "loc": "loc",
          "expr_type": "uintptr",
          "path": "string",
          "base": "uintptr",
          "import_desc": "uintptr"
        }
      },
      {
        "node_type": "literal",
        "one_of": [
          "int_literal",
          "bool_literal",
          "str_literal",
          "input",
          "output",
          "config"
        ]
      },
      {
        "node_type": "int_literal",
        "loc": "loc",
        "body": {
          "node_type": "int_literal",
          "loc": "loc",
          "expr_type": "uintptr",
          "value": "string"
        }
      },
      {
        "node_type": "bool_literal",
        "loc": "loc",
        "body": {
          "node_type": "bool_literal",
          "loc": "loc",
          "expr_type": "uintptr",
          "value": "bool"
        }
      },
      {
        "node_type": "str_literal",
        "loc": "loc",
        "body": {
          "node_type": "str_literal",
          "loc": "loc",
          "expr_type": "uintptr",
          "value": "string"
        }
      },
      {
        "node_type": "input",
        "loc": "loc",
        "body": { "node_type": "input", "loc": "loc", "expr_type": "uintptr" }
      },
      {
        "node_type": "output",
        "loc": "loc",
        "body": { "node_type": "output", "loc": "loc", "expr_type": "uintptr" }
      },
      {
        "node_type": "config",
        "loc": "loc",
        "body": { "node_type": "config", "loc": "loc", "expr_type": "uintptr" }
      },
      {
        "node_type": "stmt",
        "one_of": [
          "loop",
          "indent_scope",
          "match_branch",
          "return",
          "break",
          "continue",
          "assert",
          "implicit_yield",
          "member",
          "field",
          "format",
          "function"
        ]
      },
      {
        "node_type": "loop",
        "loc": "loc",
        "body": {
          "node_type": "loop",
          "loc": "loc",
          "init": "uintptr",
          "cond": "uintptr",
          "step": "uintptr",
          "body": "uintptr"
        }
      },
      {
        "node_type": "indent_scope",
        "loc": "loc",
        "body": {
          "node_type": "indent_scope",
          "loc": "loc",
          "elements": "array<uintptr>",
          "scope": "uintptr"
        }
      },
      {
        "node_type": "match_branch",
        "loc": "loc",
        "body": {
          "node_type": "match_branch",
          "loc": "loc",
          "cond": "uintptr",
          "sym_loc": "loc",
          "then": "uintptr"
        }
      },
      {
        "node_type": "return",
        "loc": "loc",
        "body": { "node_type": "return", "loc": "loc", "expr": "uintptr" }
      },
      {
        "node_type": "break",
        "loc": "loc",
        "body": { "node_type": "break", "loc": "loc" }
      },
      {
        "node_type": "continue",
        "loc": "loc",
        "body": { "node_type": "continue", "loc": "loc" }
      },
      {
        "node_type": "assert",
        "loc": "loc",
        "body": { "node_type": "assert", "loc": "loc", "cond": "uintptr" }
      },
      {
        "node_type": "implicit_yield",
        "loc": "loc",
        "body": { "node_type": "implicit_yield", "loc": "loc", "expr": "uintptr" }
      },
      { "node_type": "member", "one_of": ["field", "format", "function"] },
      {
        "node_type": "field",
        "loc": "loc",
        "body": {
          "node_type": "field",
          "loc": "loc",
          "ident": "uintptr",
          "colon_loc": "loc",
          "field_type": "uintptr",
          "raw_arguments": "uintptr",
          "arguments": "array<uintptr>",
          "belong": "uintptr"
        }
      },
      {
        "node_type": "format",
        "loc": "loc",
        "body": {
          "node_type": "format",
          "loc": "loc",
          "is_enum": "bool",
          "ident": "uintptr",
          "body": "uintptr",
          "belong": "uintptr",
          "struct_type": "uintptr"
        }
      },
      {
        "node_type": "function",
        "loc": "loc",
        "body": {
          "node_type": "function",
          "loc": "loc",
          "ident": "uintptr",
          "parameters": "array<uintptr>",
          "return_type": "uintptr",
          "belong": "uintptr",
          "body": "uintptr",
          "func_type": "uintptr"
        }
      },
      {
        "node_type": "type",
        "one_of": [
          "int_type",
          "ident_type",
          "int_literal_type",
          "str_literal_type",
          "void_type",
          "bool_type",
          "array_type",
          "function_type",
          "struct_type",
          "union_type"
        ]
      },
      {
        "node_type": "int_type",
        "loc": "loc",
        "body": {
          "node_type": "int_type",
          "loc": "loc",
          "bit_size": "uint",
          "endian": "endian",
          "is_signed": "bool"
        }
      },
      {
        "node_type": "ident_type",
        "loc": "loc",
        "body": {
          "node_type": "ident_type",
          "loc": "loc",
          "ident": "uintptr",
          "base": "uintptr"
        }
      },
      {
        "node_type": "int_literal_type",
        "loc": "loc",
        "body": {
          "node_type": "int_literal_type",
          "loc": "loc",
          "base": "uintptr"
        }
      },
      {
        "node_type": "str_literal_type",
        "loc": "loc",
        "body": {
          "node_type": "str_literal_type",
          "loc": "loc",
          "base": "uintptr"
        }
      },
      {
        "node_type": "void_type",
        "loc": "loc",
        "body": { "node_type": "void_type", "loc": "loc" }
      },
      {
        "node_type": "bool_type",
        "loc": "loc",
        "body": { "node_type": "bool_type", "loc": "loc" }
      },
      {
        "node_type": "array_type",
        "loc": "loc",
        "body": {
          "node_type": "array_type",
          "loc": "loc",
          "end_loc": "loc",
          "base_type": "uintptr",
          "length": "uintptr"
        }
      },
      {
        "node_type": "function_type",
        "loc": "loc",
        "body": {
          "node_type": "function_type",
          "loc": "loc",
          "return_type": "uintptr",
          "parameters": "array<uintptr>"
        }
      },
      {
        "node_type": "struct_type",
        "loc": "loc",
        "body": {
          "node_type": "struct_type",
          "loc": "loc",
          "fields": "array<uintptr>"
        }
      },
      {
        "node_type": "union_type",
        "loc": "loc",
        "body": {
          "node_type": "union_type",
          "loc": "loc",
          "fields": "array<uintptr>"
        }
      }
    ],
    "unary_op": ["!", "-"],
    "binary_op": [
      "*",
      "/",
      "%",
      "<<<",
      ">>>",
      "<<",
      ">>",
      "&",
      "+",
      "-",
      "|",
      "^",
      "==",
      "!=",
      "<",
      "<=",
      ">",
      ">=",
      "&&",
      "||",
      "if",
      "else",
      "..",
      "..=",
      "=",
      ":=",
      "::=",
      "+=",
      "-=",
      "*=",
      "/=",
      "%=",
      "<<=",
      ">>=",
      "&=",
      "|=",
      "^=",
      ","
    ],
    "ident_usage": [
      "unknown",
      "reference",
      "define_variable",
      "define_const",
      "define_field",
      "define_format",
      "define_fn",
      "reference_type"
    ],
    "endian": ["unspec", "big", "little"],
    "scope": {
      "prev": "uintptr",
      "next": "uintptr",
      "branch": "uintptr",
      "ident": "array<uintptr>"
    },
    "loc": {
      "pos": { "begin": "uint", "end": "uint" },
      "file": "uint",
      "line": "uint",
      "col": "uint"
    }
  }
  

interface parsedAST {
    ast: {
        node: parsedNode[],
        scope: parsedScope[],
    },
    file: string[],
    error: string
}

const isParsedAST = (obj: any): obj is parsedAST => {
    return obj.ast !== undefined && obj.file !== undefined && obj.error !== undefined;
}

const parse = (code: string): parsedAST => {
    const ast = JSON.parse(code);
    if (!isParsedAST(ast)) {
        throw new Error("invalid ast");
    }
    return ast;
}

interface astConstructor {
    node: Array<ast.Node>,
    scope: Array<ast.Scope>,
}

const convertToAstConstructor = (past: parsedAST): astConstructor => {
    const node = past.ast.node;
    const scope = past.ast.scope;
    const c: astConstructor = {
        node: new Array<ast.Node>(node.length),
        scope: new Array<ast.Scope>(past.ast.scope.length),
    };
    for (let i = 0; i < node.length; i++) {
        const obj: ast.Node = {
            node_type: node[i].node_type,
            loc: node[i].loc,
        }
        c.node[i] = obj;
    }
    for (let i = 0; i < scope.length; i++) {
        const obj: ast.Scope = {
            prev: null,
            next: null,
            branch: null,
            ident: []
        }
        c.scope[i] = obj;
    }
    return c
}

const convertToAST = (past: parsedAST): ast.Program | null => {
    const c = convertToAstConstructor(past);
    const node = past.ast.node;
    let p : ast.Program | null = null;
    for(let i=0;i<node.length;i++){
        const obj =node[i];
        // based on parsedSchema
        // without what holds one_of
        if(ast.isProgram(obj)){
            obj.global_scope = c.scope[past.ast.node[i].body.global_scope];
            obj.elements = [];
            if(p!==null){
                p = obj;
            }
        }
        
    }
    return p;
}