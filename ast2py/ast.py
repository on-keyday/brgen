from __future__ import annotations

from typing import Optional, List, Dict, Any

from enum import Enum


class Node:
    loc: Loc


class Expr(Node):
    expr_type: Optional[Type]


class Literal(Expr):
    pass


class Stmt(Node):
    pass


class Member(Stmt):
    pass


class Type(Node):
    pass


class Program(Node):
    struct_type: Optional[StructType]
    elements: List[Node]
    global_scope: Optional[Scope]


class Binary(Expr):
    op: BinaryOp
    left: Optional[Expr]
    right: Optional[Expr]


class Unary(Expr):
    op: UnaryOp
    expr: Optional[Expr]


class Cond(Expr):
    cond: Optional[Expr]
    then: Optional[Expr]
    els_loc: Loc
    els: Optional[Expr]


class Ident(Expr):
    ident: str
    usage: IdentUsage
    base: Optional[Node]
    scope: Optional[Scope]


class Call(Expr):
    callee: Optional[Expr]
    raw_arguments: Optional[Expr]
    arguments: List[Expr]
    end_loc: Loc


class If(Expr):
    cond: Optional[Expr]
    then: Optional[IndentScope]
    els: Optional[Node]


class MemberAccess(Expr):
    target: Optional[Expr]
    member: str
    member_loc: Loc


class Paren(Expr):
    expr: Optional[Expr]
    end_loc: Loc


class Index(Expr):
    expr: Optional[Expr]
    index: Optional[Expr]
    end_loc: Loc


class Match(Expr):
    cond: Optional[Expr]
    branch: List[Node]
    scope: Optional[Scope]


class Range(Expr):
    op: BinaryOp
    start: Optional[Expr]
    end: Optional[Expr]


class TmpVar(Expr):
    tmp_var: int


class BlockExpr(Expr):
    calls: List[Node]
    expr: Optional[Expr]


class Import(Expr):
    path: str
    base: Optional[Call]
    import_desc: Optional[Program]


class IntLiteral(Literal):
    value: str


class BoolLiteral(Literal):
    value: bool


class StrLiteral(Literal):
    value: str


class Input(Literal):
    pass


class Output(Literal):
    pass


class Config(Literal):
    pass


class Loop(Stmt):
    init: Optional[Expr]
    cond: Optional[Expr]
    step: Optional[Expr]
    body: Optional[IndentScope]


class IndentScope(Stmt):
    elements: List[Node]
    scope: Optional[Scope]


class MatchBranch(Stmt):
    cond: Optional[Expr]
    sym_loc: Loc
    then: Optional[Node]


class Return(Stmt):
    expr: Optional[Expr]


class Break(Stmt):
    pass


class Continue(Stmt):
    pass


class Assert(Stmt):
    cond: Optional[Binary]


class ImplicitYield(Stmt):
    expr: Optional[Expr]


class Field(Member):
    ident: Optional[Ident]
    colon_loc: Loc
    field_type: Optional[Type]
    raw_arguments: Optional[Expr]
    arguments: List[Expr]
    belong: Optional[Format]


class Format(Member):
    is_enum: bool
    ident: Optional[Ident]
    body: Optional[IndentScope]
    belong: Optional[Format]
    struct_type: Optional[StructType]


class Function(Member):
    ident: Optional[Ident]
    parameters: List[Field]
    return_type: Optional[Type]
    belong: Optional[Format]
    body: Optional[IndentScope]
    func_type: Optional[FunctionType]


class IntType(Type):
    bit_size: int
    endian: Endian
    is_signed: bool


class IdentType(Type):
    ident: Optional[Ident]
    base: Optional[Format]


class IntLiteralType(Type):
    base: Optional[IntLiteral]


class StrLiteralType(Type):
    base: Optional[StrLiteral]


class VoidType(Type):
    pass


class BoolType(Type):
    pass


class ArrayType(Type):
    end_loc: Loc
    base_type: Optional[Type]
    length: Optional[Expr]


class FunctionType(Type):
    return_type: Optional[Type]
    parameters: List[Type]


class StructType(Type):
    fields: List[Member]


class UnionType(Type):
    fields: List[StructType]


class Cast(Expr):
    base: Optional[Call]
    expr: Optional[Expr]


class Comment(Node):
    comment: str


class CommentGroup(Node):
    comments: List[Comment]


class UnaryOp(Enum):
    NOT = "!"
    MINUS_SIGN = "-"


class BinaryOp(Enum):
    MUL = "*"
    DIV = "/"
    MOD = "%"
    LEFT_ARITHMETIC_SHIFT = "<<<"
    RIGHT_ARITHMETIC_SHIFT = ">>>"
    LEFT_LOGICAL_SHIFT = "<<"
    RIGHT_LOGICAL_SHIFT = ">>"
    BIT_AND = "&"
    ADD = "+"
    SUB = "-"
    BIT_OR = "|"
    BIT_XOR = "^"
    EQUAL = "=="
    NOT_EQUAL = "!="
    LESS = "<"
    LESS_OR_EQ = "<="
    GRATER = ">"
    GRATER_OR_EQ = ">="
    LOGICAL_AND = "&&"
    LOGICAL_OR = "||"
    COND_OP_1 = "if"
    COND_OP_2 = "else"
    RANGE_EXCLUSIVE = ".."
    RANGE_INCLUSIVE = "..="
    ASSIGN = "="
    DEFINE_ASSIGN = ":="
    CONST_ASSIGN = "::="
    ADD_ASSIGN = "+="
    SUB_ASSIGN = "-="
    MUL_ASSIGN = "*="
    DIV_ASSIGN = "/="
    MOD_ASSIGN = "%="
    LEFT_SHIFT_ASSIGN = "<<="
    RIGHT_SHIFT_ASSIGN = ">>="
    BIT_AND_ASSIGN = "&="
    BIT_OR_ASSIGN = "|="
    BIT_XOR_ASSIGN = "^="
    COMMA = ","


class IdentUsage(Enum):
    UNKNOWN = "unknown"
    REFERENCE = "reference"
    DEFINE_VARIABLE = "define_variable"
    DEFINE_CONST = "define_const"
    DEFINE_FIELD = "define_field"
    DEFINE_FORMAT = "define_format"
    DEFINE_ENUM = "define_enum"
    DEFINE_FN = "define_fn"
    DEFINE_ARG = "define_arg"
    REFERENCE_TYPE = "reference_type"


class Endian(Enum):
    UNSPEC = "unspec"
    BIG = "big"
    LITTLE = "little"


class TokenTag(Enum):
    INDENT = "indent"
    SPACE = "space"
    LINE = "line"
    PUNCT = "punct"
    INT_LITERAL = "int_literal"
    BOOL_LITERAL = "bool_literal"
    STR_LITERAL = "str_literal"
    KEYWORD = "keyword"
    IDENT = "ident"
    COMMENT = "comment"
    ERROR = "error"
    UNKNOWN = "unknown"


class Scope:
    prev: Optional[Scope]
    next: Optional[Scope]
    branch: Optional[Scope]
    ident: List[Ident]


class Loc:
    pos: Pos
    file: int
    line: int
    col: int


class Pos:
    begin: int
    end: int


class Token:
    tag: TokenTag
    token: str
    loc: Loc


class SrcErrorEntry:
    msg: str
    file: str
    loc: Loc
    src: str
    warn: bool


class SrcError:
    errs: List[SrcErrorEntry]


class RawNode:
    node_type: str
    loc: Loc
    body: Dict[str, Any]


class RawScope:
    prev: Optional[int]
    next: Optional[int]
    branch: Optional[int]
    ident: List[int]


class Ast:
    node: List[RawNode]
    scope: List[RawScope]


class AstFile:
    files: List[str]
    ast: Optional[Ast]
    error: Optional[SrcError]


class TokenFile:
    files: List[str]
    tokens: List[Token]
    error: Optional[SrcError]
