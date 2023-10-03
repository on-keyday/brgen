
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


const parseSchema = { "node": [{ "node_type": "node", "one_of": ["program", "expr", "binary", "unary", "cond", "ident", "call", "if", "member_access", "paren", "index", "match", "range", "tmp_var", "block_expr", "import", "literal", "int_literal", "bool_literal", "str_literal", "input", "output", "config", "stmt", "loop", "indent_scope", "match_branch", "return", "break", "continue", "assert", "implicit_yield", "member", "field", "format", "function", "type", "int_type", "ident_type", "int_literal_type", "str_literal_type", "void_type", "bool_type", "array_type", "function_type", "struct_type", "union_type"] }, { "node_type": "program", "loc": "loc", "struct_type": "uintptr", "elements": "array<uintptr>", "global_scope": "uintptr" }, { "node_type": "expr", "one_of": ["binary", "unary", "cond", "ident", "call", "if", "member_access", "paren", "index", "match", "range", "tmp_var", "block_expr", "import", "literal", "int_literal", "bool_literal", "str_literal", "input", "output", "config"] }, { "node_type": "binary", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "op": "binary_op", "left": "uintptr", "right": "uintptr" }, { "node_type": "unary", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "op": "unary_op", "expr": "uintptr" }, { "node_type": "cond", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "cond": "uintptr", "then": "uintptr", "els_loc": "loc", "els": "uintptr" }, { "node_type": "ident", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "ident": "string", "usage": "ident_usage", "base": "uintptr", "scope": "uintptr" }, { "node_type": "call", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "callee": "uintptr", "raw_arguments": "uintptr", "arguments": "array<uintptr>", "end_loc": "loc" }, { "node_type": "if", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "cond": "uintptr", "then": "uintptr", "els": "uintptr" }, { "node_type": "member_access", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "target": "uintptr", "member": "string", "member_loc": "loc" }, { "node_type": "paren", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "expr": "uintptr", "end_loc": "loc" }, { "node_type": "index", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "expr": "uintptr", "index": "uintptr", "end_loc": "loc" }, { "node_type": "match", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "cond": "uintptr", "branch": "array<uintptr>", "scope": "uintptr" }, { "node_type": "range", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "op": "binary_op", "start": "uintptr", "end": "uintptr" }, { "node_type": "tmp_var", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "tmp_var": "uint" }, { "node_type": "block_expr", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "calls": "array<uintptr>", "expr": "uintptr" }, { "node_type": "import", "base_node_type": ["expr"], "loc": "loc", "expr_type": "uintptr", "path": "string", "base": "uintptr", "import_desc": "uintptr" }, { "node_type": "literal", "one_of": ["int_literal", "bool_literal", "str_literal", "input", "output", "config"] }, { "node_type": "int_literal", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr", "value": "string" }, { "node_type": "bool_literal", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr", "value": "bool" }, { "node_type": "str_literal", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr", "value": "string" }, { "node_type": "input", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr" }, { "node_type": "output", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr" }, { "node_type": "config", "base_node_type": ["literal", "expr"], "loc": "loc", "expr_type": "uintptr" }, { "node_type": "stmt", "one_of": ["loop", "indent_scope", "match_branch", "return", "break", "continue", "assert", "implicit_yield", "member", "field", "format", "function"] }, { "node_type": "loop", "base_node_type": ["stmt"], "loc": "loc", "init": "uintptr", "cond": "uintptr", "step": "uintptr", "body": "uintptr" }, { "node_type": "indent_scope", "base_node_type": ["stmt"], "loc": "loc", "elements": "array<uintptr>", "scope": "uintptr" }, { "node_type": "match_branch", "base_node_type": ["stmt"], "loc": "loc", "cond": "uintptr", "sym_loc": "loc", "then": "uintptr" }, { "node_type": "return", "base_node_type": ["stmt"], "loc": "loc", "expr": "uintptr" }, { "node_type": "break", "base_node_type": ["stmt"], "loc": "loc" }, { "node_type": "continue", "base_node_type": ["stmt"], "loc": "loc" }, { "node_type": "assert", "base_node_type": ["stmt"], "loc": "loc", "cond": "uintptr" }, { "node_type": "implicit_yield", "base_node_type": ["stmt"], "loc": "loc", "expr": "uintptr" }, { "node_type": "member", "one_of": ["field", "format", "function"] }, { "node_type": "field", "base_node_type": ["member", "stmt"], "loc": "loc", "ident": "uintptr", "colon_loc": "loc", "field_type": "uintptr", "raw_arguments": "uintptr", "arguments": "array<uintptr>", "belong": "uintptr" }, { "node_type": "format", "base_node_type": ["member", "stmt"], "loc": "loc", "is_enum": "bool", "ident": "uintptr", "body": "uintptr", "belong": "uintptr", "struct_type": "uintptr" }, { "node_type": "function", "base_node_type": ["member", "stmt"], "loc": "loc", "ident": "uintptr", "parameters": "array<uintptr>", "return_type": "uintptr", "belong": "uintptr", "body": "uintptr", "func_type": "uintptr" }, { "node_type": "type", "one_of": ["int_type", "ident_type", "int_literal_type", "str_literal_type", "void_type", "bool_type", "array_type", "function_type", "struct_type", "union_type"] }, { "node_type": "int_type", "base_node_type": ["type"], "loc": "loc", "bit_size": "uint", "endian": "endian", "is_signed": "bool" }, { "node_type": "ident_type", "base_node_type": ["type"], "loc": "loc", "ident": "uintptr", "base": "uintptr" }, { "node_type": "int_literal_type", "base_node_type": ["type"], "loc": "loc", "base": "uintptr" }, { "node_type": "str_literal_type", "base_node_type": ["type"], "loc": "loc", "base": "uintptr" }, { "node_type": "void_type", "base_node_type": ["type"], "loc": "loc" }, { "node_type": "bool_type", "base_node_type": ["type"], "loc": "loc" }, { "node_type": "array_type", "base_node_type": ["type"], "loc": "loc", "end_loc": "loc", "base_type": "uintptr", "length": "uintptr" }, { "node_type": "function_type", "base_node_type": ["type"], "loc": "loc", "return_type": "uintptr", "parameters": "array<uintptr>" }, { "node_type": "struct_type", "base_node_type": ["type"], "loc": "loc", "fields": "array<uintptr>" }, { "node_type": "union_type", "base_node_type": ["type"], "loc": "loc", "fields": "array<uintptr>" }], "unary_op": ["!", "-"], "binary_op": ["*", "/", "%", "<<<", ">>>", "<<", ">>", "&", "+", "-", "|", "^", "==", "!=", "<", "<=", ">", ">=", "&&", "||", "if", "else", "..", "..=", "=", ":=", "::=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=", ","], "ident_usage": ["unknown", "reference", "define_variable", "define_const", "define_field", "define_format", "define_fn", "reference_type"], "endian": ["unspec", "big", "little"], "scope": { "prev": "uintptr", "next": "uintptr", "branch": "uintptr", "ident": "array<uintptr>" }, "loc": { "pos": { "begin": "uint", "end": "uint" }, "file": "uint", "line": "uint", "col": "uint" } }


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
        if(ast.isProgram(obj)){
            obj.global_scope = c.scope[past.ast.node[i].body.global_scope];
            obj.elements = [];
            if(p!==null){
                p = obj;
            }
        }
        else if(ast.is)
    }
    return p;
}