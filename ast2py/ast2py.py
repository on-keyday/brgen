from __future__ import annotations

from typing import Optional,List,Dict,Any,Callable

from enum import Enum as PyEnum

class NodeType(PyEnum):
    PROGRAM = "program"
    COMMENT = "comment"
    COMMENT_GROUP = "comment_group"
    EXPR = "expr"
    BINARY = "binary"
    UNARY = "unary"
    COND = "cond"
    IDENT = "ident"
    CALL = "call"
    IF = "if"
    MEMBER_ACCESS = "member_access"
    PAREN = "paren"
    INDEX = "index"
    MATCH = "match"
    RANGE = "range"
    TMP_VAR = "tmp_var"
    BLOCK_EXPR = "block_expr"
    IMPORT = "import"
    CAST = "cast"
    STMT = "stmt"
    LOOP = "loop"
    INDENT_BLOCK = "indent_block"
    MATCH_BRANCH = "match_branch"
    UNION_CANDIDATE = "union_candidate"
    RETURN = "return"
    BREAK = "break"
    CONTINUE = "continue"
    ASSERT = "assert"
    IMPLICIT_YIELD = "implicit_yield"
    TYPE = "type"
    INT_TYPE = "int_type"
    IDENT_TYPE = "ident_type"
    INT_LITERAL_TYPE = "int_literal_type"
    STR_LITERAL_TYPE = "str_literal_type"
    VOID_TYPE = "void_type"
    BOOL_TYPE = "bool_type"
    ARRAY_TYPE = "array_type"
    FUNCTION_TYPE = "function_type"
    STRUCT_TYPE = "struct_type"
    STRUCT_UNION_TYPE = "struct_union_type"
    UNION_TYPE = "union_type"
    RANGE_TYPE = "range_type"
    ENUM_TYPE = "enum_type"
    LITERAL = "literal"
    INT_LITERAL = "int_literal"
    BOOL_LITERAL = "bool_literal"
    STR_LITERAL = "str_literal"
    INPUT = "input"
    OUTPUT = "output"
    CONFIG = "config"
    MEMBER = "member"
    FIELD = "field"
    FORMAT = "format"
    STATE = "state"
    ENUM = "enum"
    ENUM_MEMBER = "enum_member"
    FUNCTION = "function"
    BUILTIN_FUNCTION = "builtin_function"


class UnaryOp(PyEnum):
    NOT = "!"
    MINUS_SIGN = "-"


class BinaryOp(PyEnum):
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
    COND_OP_1 = "?"
    COND_OP_2 = ":"
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


class IdentUsage(PyEnum):
    UNKNOWN = "unknown"
    REFERENCE = "reference"
    DEFINE_VARIABLE = "define_variable"
    DEFINE_CONST = "define_const"
    DEFINE_FIELD = "define_field"
    DEFINE_FORMAT = "define_format"
    DEFINE_STATE = "define_state"
    DEFINE_ENUM = "define_enum"
    DEFINE_ENUM_MEMBER = "define_enum_member"
    DEFINE_FN = "define_fn"
    DEFINE_CAST_FN = "define_cast_fn"
    DEFINE_ARG = "define_arg"
    REFERENCE_TYPE = "reference_type"
    REFERENCE_MEMBER = "reference_member"
    MAYBE_TYPE = "maybe_type"


class Endian(PyEnum):
    UNSPEC = "unspec"
    BIG = "big"
    LITTLE = "little"


class TokenTag(PyEnum):
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


class ConstantLevel(PyEnum):
    UNKNOWN = "unknown"
    CONST_VALUE = "const_value"
    CONST_VARIABLE = "const_variable"
    VARIABLE = "variable"


class BitAlignment(PyEnum):
    BYTE_ALIGNED = "byte_aligned"
    BIT_1 = "bit_1"
    BIT_2 = "bit_2"
    BIT_3 = "bit_3"
    BIT_4 = "bit_4"
    BIT_5 = "bit_5"
    BIT_6 = "bit_6"
    BIT_7 = "bit_7"
    NOT_TARGET = "not_target"
    NOT_DECIDABLE = "not_decidable"


class Node:
    loc: Loc


class Expr(Node):
    expr_type: Optional[Type]
    constant_level: ConstantLevel


class Stmt(Node):
    pass


class Type(Node):
    is_explicit: bool
    is_int_set: bool
    bit_alignment: BitAlignment
    bit_size: int


class Literal(Expr):
    pass


class Member(Stmt):
    belong: Optional[Member]
    ident: Optional[Ident]


class Program(Node):
    struct_type: Optional[StructType]
    elements: List[Node]
    global_scope: Optional[Scope]


class Comment(Node):
    comment: str


class CommentGroup(Node):
    comments: List[Comment]


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
    cond_scope: Optional[Scope]
    cond: Optional[Expr]
    then: Optional[IndentBlock]
    els: Optional[Node]


class MemberAccess(Expr):
    target: Optional[Expr]
    member: Optional[Ident]
    base: Optional[Node]


class Paren(Expr):
    expr: Optional[Expr]
    end_loc: Loc


class Index(Expr):
    expr: Optional[Expr]
    index: Optional[Expr]
    end_loc: Loc


class Match(Expr):
    cond_scope: Optional[Scope]
    cond: Optional[Expr]
    branch: List[Node]


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


class Cast(Expr):
    base: Optional[Call]
    expr: Optional[Expr]


class Loop(Stmt):
    cond_scope: Optional[Scope]
    init: Optional[Expr]
    cond: Optional[Expr]
    step: Optional[Expr]
    body: Optional[IndentBlock]


class IndentBlock(Stmt):
    struct_type: Optional[StructType]
    elements: List[Node]
    scope: Optional[Scope]


class MatchBranch(Stmt):
    cond: Optional[Expr]
    sym_loc: Loc
    then: Optional[Node]


class UnionCandidate(Stmt):
    cond: Optional[Expr]
    field: Optional[Field]


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


class IntType(Type):
    endian: Endian
    is_signed: bool
    is_common_supported: bool


class IdentType(Type):
    ident: Optional[Ident]
    base: Optional[Type]


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
    base: Optional[Node]
    recursive: bool


class StructUnionType(Type):
    fields: List[StructType]
    base: Optional[Expr]
    union_fields: List[Field]


class UnionType(Type):
    cond: Optional[Expr]
    candidates: List[UnionCandidate]
    base_type: Optional[StructUnionType]
    common_type: Optional[Type]


class RangeType(Type):
    base_type: Optional[Type]
    range: Optional[Range]


class EnumType(Type):
    base: Optional[Enum]


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


class Field(Member):
    colon_loc: Loc
    field_type: Optional[Type]
    raw_arguments: Optional[Expr]
    arguments: List[Expr]
    bit_alignment: BitAlignment


class Format(Member):
    body: Optional[IndentBlock]


class State(Member):
    body: Optional[IndentBlock]


class Enum(Member):
    scope: Optional[Scope]
    colon_loc: Loc
    base_type: Optional[Type]
    members: List[EnumMember]
    enum_type: Optional[EnumType]


class EnumMember(Member):
    expr: Optional[Expr]


class Function(Member):
    parameters: List[Field]
    return_type: Optional[Type]
    body: Optional[IndentBlock]
    func_type: Optional[FunctionType]
    is_cast: bool
    cast_loc: Loc


class BuiltinFunction(Member):
    func_type: Optional[FunctionType]


class Scope:
    prev: Optional[Scope]
    next: Optional[Scope]
    branch: Optional[Scope]
    ident: List[Ident]
    owner: Optional[Node]
    branch_root: bool


class Pos:
    begin: int
    end: int

def parse_Pos(json: dict) -> Pos:
    ret = Pos()
    ret.begin = int(json["begin"])
    ret.end = int(json["end"])
    return ret



class Loc:
    pos: Pos
    file: int
    line: int
    col: int

def parse_Loc(json: dict) -> Loc:
    ret = Loc()
    ret.pos = parse_Pos(json["pos"])
    ret.file = int(json["file"])
    ret.line = int(json["line"])
    ret.col = int(json["col"])
    return ret



class Token:
    tag: TokenTag
    token: str
    loc: Loc

def parse_Token(json: dict) -> Token:
    ret = Token()
    ret.tag = TokenTag(json["tag"])
    ret.token = str(json["token"])
    ret.loc = parse_Loc(json["loc"])
    return ret



class RawScope:
    prev: Optional[int]
    next: Optional[int]
    branch: Optional[int]
    ident: List[int]
    owner: Optional[int]
    branch_root: bool

def parse_RawScope(json: dict) -> RawScope:
    ret = RawScope()
    if json["prev"] is not None:
        ret.prev = int(json["prev"])
    else:
        ret.prev = None
    if json["next"] is not None:
        ret.next = int(json["next"])
    else:
        ret.next = None
    if json["branch"] is not None:
        ret.branch = int(json["branch"])
    else:
        ret.branch = None
    ret.ident = [int(x) for x in json["ident"]]
    if json["owner"] is not None:
        ret.owner = int(json["owner"])
    else:
        ret.owner = None
    ret.branch_root = bool(json["branch_root"])
    return ret



class RawNode:
    node_type: NodeType
    loc: Loc
    body: Any

def parse_RawNode(json: dict) -> RawNode:
    ret = RawNode()
    ret.node_type = NodeType(json["node_type"])
    ret.loc = parse_Loc(json["loc"])
    ret.body = json["body"]
    return ret



class SrcErrorEntry:
    msg: str
    file: str
    loc: Loc
    src: str
    warn: bool

def parse_SrcErrorEntry(json: dict) -> SrcErrorEntry:
    ret = SrcErrorEntry()
    ret.msg = str(json["msg"])
    ret.file = str(json["file"])
    ret.loc = parse_Loc(json["loc"])
    ret.src = str(json["src"])
    ret.warn = bool(json["warn"])
    return ret



class SrcError:
    errs: List[SrcErrorEntry]

def parse_SrcError(json: dict) -> SrcError:
    ret = SrcError()
    ret.errs = [parse_SrcErrorEntry(x) for x in json["errs"]]
    return ret



class JsonAst:
    node: List[RawNode]
    scope: List[RawScope]

def parse_JsonAst(json: dict) -> JsonAst:
    ret = JsonAst()
    ret.node = [parse_RawNode(x) for x in json["node"]]
    ret.scope = [parse_RawScope(x) for x in json["scope"]]
    return ret



class AstFile:
    files: List[str]
    ast: Optional[JsonAst]
    error: Optional[SrcError]

def parse_AstFile(json: dict) -> AstFile:
    ret = AstFile()
    ret.files = [str(x) for x in json["files"]]
    if json["ast"] is not None:
        ret.ast = parse_JsonAst(json["ast"])
    else:
        ret.ast = None
    if json["error"] is not None:
        ret.error = parse_SrcError(json["error"])
    else:
        ret.error = None
    return ret



class TokenFile:
    files: List[str]
    tokens: Optional[List[Token]]
    error: Optional[SrcError]

def parse_TokenFile(json: dict) -> TokenFile:
    ret = TokenFile()
    ret.files = [str(x) for x in json["files"]]
    if json["tokens"] is not None:
        ret.tokens = [parse_Token(x) for x in json["tokens"]]
    else:
        ret.tokens = None
    if json["error"] is not None:
        ret.error = parse_SrcError(json["error"])
    else:
        ret.error = None
    return ret



def raiseError(err):
    raise err

def ast2node(ast :JsonAst) -> Program:
    if not isinstance(ast,JsonAst):
        raise TypeError('ast must be JsonAst')
    node = list()
    for raw in ast.node:
        match raw.node_type:
            case NodeType.PROGRAM:
                node.append(Program())
            case NodeType.COMMENT:
                node.append(Comment())
            case NodeType.COMMENT_GROUP:
                node.append(CommentGroup())
            case NodeType.BINARY:
                node.append(Binary())
            case NodeType.UNARY:
                node.append(Unary())
            case NodeType.COND:
                node.append(Cond())
            case NodeType.IDENT:
                node.append(Ident())
            case NodeType.CALL:
                node.append(Call())
            case NodeType.IF:
                node.append(If())
            case NodeType.MEMBER_ACCESS:
                node.append(MemberAccess())
            case NodeType.PAREN:
                node.append(Paren())
            case NodeType.INDEX:
                node.append(Index())
            case NodeType.MATCH:
                node.append(Match())
            case NodeType.RANGE:
                node.append(Range())
            case NodeType.TMP_VAR:
                node.append(TmpVar())
            case NodeType.BLOCK_EXPR:
                node.append(BlockExpr())
            case NodeType.IMPORT:
                node.append(Import())
            case NodeType.CAST:
                node.append(Cast())
            case NodeType.LOOP:
                node.append(Loop())
            case NodeType.INDENT_BLOCK:
                node.append(IndentBlock())
            case NodeType.MATCH_BRANCH:
                node.append(MatchBranch())
            case NodeType.UNION_CANDIDATE:
                node.append(UnionCandidate())
            case NodeType.RETURN:
                node.append(Return())
            case NodeType.BREAK:
                node.append(Break())
            case NodeType.CONTINUE:
                node.append(Continue())
            case NodeType.ASSERT:
                node.append(Assert())
            case NodeType.IMPLICIT_YIELD:
                node.append(ImplicitYield())
            case NodeType.INT_TYPE:
                node.append(IntType())
            case NodeType.IDENT_TYPE:
                node.append(IdentType())
            case NodeType.INT_LITERAL_TYPE:
                node.append(IntLiteralType())
            case NodeType.STR_LITERAL_TYPE:
                node.append(StrLiteralType())
            case NodeType.VOID_TYPE:
                node.append(VoidType())
            case NodeType.BOOL_TYPE:
                node.append(BoolType())
            case NodeType.ARRAY_TYPE:
                node.append(ArrayType())
            case NodeType.FUNCTION_TYPE:
                node.append(FunctionType())
            case NodeType.STRUCT_TYPE:
                node.append(StructType())
            case NodeType.STRUCT_UNION_TYPE:
                node.append(StructUnionType())
            case NodeType.UNION_TYPE:
                node.append(UnionType())
            case NodeType.RANGE_TYPE:
                node.append(RangeType())
            case NodeType.ENUM_TYPE:
                node.append(EnumType())
            case NodeType.INT_LITERAL:
                node.append(IntLiteral())
            case NodeType.BOOL_LITERAL:
                node.append(BoolLiteral())
            case NodeType.STR_LITERAL:
                node.append(StrLiteral())
            case NodeType.INPUT:
                node.append(Input())
            case NodeType.OUTPUT:
                node.append(Output())
            case NodeType.CONFIG:
                node.append(Config())
            case NodeType.FIELD:
                node.append(Field())
            case NodeType.FORMAT:
                node.append(Format())
            case NodeType.STATE:
                node.append(State())
            case NodeType.ENUM:
                node.append(Enum())
            case NodeType.ENUM_MEMBER:
                node.append(EnumMember())
            case NodeType.FUNCTION:
                node.append(Function())
            case NodeType.BUILTIN_FUNCTION:
                node.append(BuiltinFunction())
            case _:
                raise TypeError('unknown node type')
    scope = [Scope() for _ in range(len(ast.scope))]
    for i in range(len(ast.node)):
        node[i].loc = ast.node[i].loc
        match ast.node[i].node_type:
            case NodeType.PROGRAM:
                if ast.node[i].body["struct_type"] is not None:
                    x = node[ast.node[i].body["struct_type"]]
                    node[i].struct_type = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at Program::struct_type'))
                else:
                    node[i].struct_type = None
                node[i].elements = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch at Program::elements'))) for x in ast.node[i].body["elements"]]
                if ast.node[i].body["global_scope"] is not None:
                    node[i].global_scope = scope[ast.node[i].body["global_scope"]]
                else:
                    node[i].global_scope = None
            case NodeType.COMMENT:
                x = ast.node[i].body["comment"]
                node[i].comment = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at Comment::comment'))
            case NodeType.COMMENT_GROUP:
                node[i].comments = [(node[x] if isinstance(node[x],Comment) else raiseError(TypeError('type mismatch at CommentGroup::comments'))) for x in ast.node[i].body["comments"]]
            case NodeType.BINARY:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Binary::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                node[i].op = BinaryOp(ast.node[i].body["op"])
                if ast.node[i].body["left"] is not None:
                    x = node[ast.node[i].body["left"]]
                    node[i].left = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Binary::left'))
                else:
                    node[i].left = None
                if ast.node[i].body["right"] is not None:
                    x = node[ast.node[i].body["right"]]
                    node[i].right = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Binary::right'))
                else:
                    node[i].right = None
            case NodeType.UNARY:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Unary::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                node[i].op = UnaryOp(ast.node[i].body["op"])
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Unary::expr'))
                else:
                    node[i].expr = None
            case NodeType.COND:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Cond::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Cond::cond'))
                else:
                    node[i].cond = None
                if ast.node[i].body["then"] is not None:
                    x = node[ast.node[i].body["then"]]
                    node[i].then = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Cond::then'))
                else:
                    node[i].then = None
                node[i].els_loc = parse_Loc(ast.node[i].body["els_loc"])
                if ast.node[i].body["els"] is not None:
                    x = node[ast.node[i].body["els"]]
                    node[i].els = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Cond::els'))
                else:
                    node[i].els = None
            case NodeType.IDENT:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Ident::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["ident"]
                node[i].ident = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at Ident::ident'))
                node[i].usage = IdentUsage(ast.node[i].body["usage"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at Ident::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["scope"] is not None:
                    node[i].scope = scope[ast.node[i].body["scope"]]
                else:
                    node[i].scope = None
            case NodeType.CALL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Call::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["callee"] is not None:
                    x = node[ast.node[i].body["callee"]]
                    node[i].callee = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Call::callee'))
                else:
                    node[i].callee = None
                if ast.node[i].body["raw_arguments"] is not None:
                    x = node[ast.node[i].body["raw_arguments"]]
                    node[i].raw_arguments = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Call::raw_arguments'))
                else:
                    node[i].raw_arguments = None
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at Call::arguments'))) for x in ast.node[i].body["arguments"]]
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.IF:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at If::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["cond_scope"] is not None:
                    node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                else:
                    node[i].cond_scope = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at If::cond'))
                else:
                    node[i].cond = None
                if ast.node[i].body["then"] is not None:
                    x = node[ast.node[i].body["then"]]
                    node[i].then = x if isinstance(x,IndentBlock) else raiseError(TypeError('type mismatch at If::then'))
                else:
                    node[i].then = None
                if ast.node[i].body["els"] is not None:
                    x = node[ast.node[i].body["els"]]
                    node[i].els = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at If::els'))
                else:
                    node[i].els = None
            case NodeType.MEMBER_ACCESS:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at MemberAccess::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["target"] is not None:
                    x = node[ast.node[i].body["target"]]
                    node[i].target = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at MemberAccess::target'))
                else:
                    node[i].target = None
                if ast.node[i].body["member"] is not None:
                    x = node[ast.node[i].body["member"]]
                    node[i].member = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at MemberAccess::member'))
                else:
                    node[i].member = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at MemberAccess::base'))
                else:
                    node[i].base = None
            case NodeType.PAREN:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Paren::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Paren::expr'))
                else:
                    node[i].expr = None
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.INDEX:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Index::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Index::expr'))
                else:
                    node[i].expr = None
                if ast.node[i].body["index"] is not None:
                    x = node[ast.node[i].body["index"]]
                    node[i].index = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Index::index'))
                else:
                    node[i].index = None
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.MATCH:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Match::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["cond_scope"] is not None:
                    node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                else:
                    node[i].cond_scope = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Match::cond'))
                else:
                    node[i].cond = None
                node[i].branch = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch at Match::branch'))) for x in ast.node[i].body["branch"]]
            case NodeType.RANGE:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Range::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                node[i].op = BinaryOp(ast.node[i].body["op"])
                if ast.node[i].body["start"] is not None:
                    x = node[ast.node[i].body["start"]]
                    node[i].start = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Range::start'))
                else:
                    node[i].start = None
                if ast.node[i].body["end"] is not None:
                    x = node[ast.node[i].body["end"]]
                    node[i].end = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Range::end'))
                else:
                    node[i].end = None
            case NodeType.TMP_VAR:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at TmpVar::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["tmp_var"]
                node[i].tmp_var = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at TmpVar::tmp_var'))
            case NodeType.BLOCK_EXPR:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at BlockExpr::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                node[i].calls = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch at BlockExpr::calls'))) for x in ast.node[i].body["calls"]]
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at BlockExpr::expr'))
                else:
                    node[i].expr = None
            case NodeType.IMPORT:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Import::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["path"]
                node[i].path = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at Import::path'))
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Call) else raiseError(TypeError('type mismatch at Import::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["import_desc"] is not None:
                    x = node[ast.node[i].body["import_desc"]]
                    node[i].import_desc = x if isinstance(x,Program) else raiseError(TypeError('type mismatch at Import::import_desc'))
                else:
                    node[i].import_desc = None
            case NodeType.CAST:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Cast::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Call) else raiseError(TypeError('type mismatch at Cast::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Cast::expr'))
                else:
                    node[i].expr = None
            case NodeType.LOOP:
                if ast.node[i].body["cond_scope"] is not None:
                    node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                else:
                    node[i].cond_scope = None
                if ast.node[i].body["init"] is not None:
                    x = node[ast.node[i].body["init"]]
                    node[i].init = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Loop::init'))
                else:
                    node[i].init = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Loop::cond'))
                else:
                    node[i].cond = None
                if ast.node[i].body["step"] is not None:
                    x = node[ast.node[i].body["step"]]
                    node[i].step = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Loop::step'))
                else:
                    node[i].step = None
                if ast.node[i].body["body"] is not None:
                    x = node[ast.node[i].body["body"]]
                    node[i].body = x if isinstance(x,IndentBlock) else raiseError(TypeError('type mismatch at Loop::body'))
                else:
                    node[i].body = None
            case NodeType.INDENT_BLOCK:
                if ast.node[i].body["struct_type"] is not None:
                    x = node[ast.node[i].body["struct_type"]]
                    node[i].struct_type = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at IndentBlock::struct_type'))
                else:
                    node[i].struct_type = None
                node[i].elements = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch at IndentBlock::elements'))) for x in ast.node[i].body["elements"]]
                if ast.node[i].body["scope"] is not None:
                    node[i].scope = scope[ast.node[i].body["scope"]]
                else:
                    node[i].scope = None
            case NodeType.MATCH_BRANCH:
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at MatchBranch::cond'))
                else:
                    node[i].cond = None
                node[i].sym_loc = parse_Loc(ast.node[i].body["sym_loc"])
                if ast.node[i].body["then"] is not None:
                    x = node[ast.node[i].body["then"]]
                    node[i].then = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at MatchBranch::then'))
                else:
                    node[i].then = None
            case NodeType.UNION_CANDIDATE:
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at UnionCandidate::cond'))
                else:
                    node[i].cond = None
                if ast.node[i].body["field"] is not None:
                    x = node[ast.node[i].body["field"]]
                    node[i].field = x if isinstance(x,Field) else raiseError(TypeError('type mismatch at UnionCandidate::field'))
                else:
                    node[i].field = None
            case NodeType.RETURN:
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Return::expr'))
                else:
                    node[i].expr = None
            case NodeType.BREAK:
                pass
            case NodeType.CONTINUE:
                pass
            case NodeType.ASSERT:
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Binary) else raiseError(TypeError('type mismatch at Assert::cond'))
                else:
                    node[i].cond = None
            case NodeType.IMPLICIT_YIELD:
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at ImplicitYield::expr'))
                else:
                    node[i].expr = None
            case NodeType.INT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at IntType::bit_size'))
                node[i].endian = Endian(ast.node[i].body["endian"])
                x = ast.node[i].body["is_signed"]
                node[i].is_signed = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_signed'))
                x = ast.node[i].body["is_common_supported"]
                node[i].is_common_supported = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_common_supported'))
            case NodeType.IDENT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IdentType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IdentType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at IdentType::bit_size'))
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at IdentType::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at IdentType::base'))
                else:
                    node[i].base = None
            case NodeType.INT_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntLiteralType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntLiteralType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at IntLiteralType::bit_size'))
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,IntLiteral) else raiseError(TypeError('type mismatch at IntLiteralType::base'))
                else:
                    node[i].base = None
            case NodeType.STR_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StrLiteralType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StrLiteralType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StrLiteralType::bit_size'))
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,StrLiteral) else raiseError(TypeError('type mismatch at StrLiteralType::base'))
                else:
                    node[i].base = None
            case NodeType.VOID_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at VoidType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at VoidType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at VoidType::bit_size'))
            case NodeType.BOOL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at BoolType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at BoolType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at BoolType::bit_size'))
            case NodeType.ARRAY_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at ArrayType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at ArrayType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at ArrayType::bit_size'))
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
                if ast.node[i].body["base_type"] is not None:
                    x = node[ast.node[i].body["base_type"]]
                    node[i].base_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at ArrayType::base_type'))
                else:
                    node[i].base_type = None
                if ast.node[i].body["length"] is not None:
                    x = node[ast.node[i].body["length"]]
                    node[i].length = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at ArrayType::length'))
                else:
                    node[i].length = None
            case NodeType.FUNCTION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FunctionType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FunctionType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at FunctionType::bit_size'))
                if ast.node[i].body["return_type"] is not None:
                    x = node[ast.node[i].body["return_type"]]
                    node[i].return_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at FunctionType::return_type'))
                else:
                    node[i].return_type = None
                node[i].parameters = [(node[x] if isinstance(node[x],Type) else raiseError(TypeError('type mismatch at FunctionType::parameters'))) for x in ast.node[i].body["parameters"]]
            case NodeType.STRUCT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StructType::bit_size'))
                node[i].fields = [(node[x] if isinstance(node[x],Member) else raiseError(TypeError('type mismatch at StructType::fields'))) for x in ast.node[i].body["fields"]]
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at StructType::base'))
                else:
                    node[i].base = None
                x = ast.node[i].body["recursive"]
                node[i].recursive = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::recursive'))
            case NodeType.STRUCT_UNION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructUnionType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructUnionType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StructUnionType::bit_size'))
                node[i].fields = [(node[x] if isinstance(node[x],StructType) else raiseError(TypeError('type mismatch at StructUnionType::fields'))) for x in ast.node[i].body["fields"]]
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at StructUnionType::base'))
                else:
                    node[i].base = None
                node[i].union_fields = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch at StructUnionType::union_fields'))) for x in ast.node[i].body["union_fields"]]
            case NodeType.UNION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at UnionType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at UnionType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at UnionType::bit_size'))
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at UnionType::cond'))
                else:
                    node[i].cond = None
                node[i].candidates = [(node[x] if isinstance(node[x],UnionCandidate) else raiseError(TypeError('type mismatch at UnionType::candidates'))) for x in ast.node[i].body["candidates"]]
                if ast.node[i].body["base_type"] is not None:
                    x = node[ast.node[i].body["base_type"]]
                    node[i].base_type = x if isinstance(x,StructUnionType) else raiseError(TypeError('type mismatch at UnionType::base_type'))
                else:
                    node[i].base_type = None
                if ast.node[i].body["common_type"] is not None:
                    x = node[ast.node[i].body["common_type"]]
                    node[i].common_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at UnionType::common_type'))
                else:
                    node[i].common_type = None
            case NodeType.RANGE_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RangeType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RangeType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at RangeType::bit_size'))
                if ast.node[i].body["base_type"] is not None:
                    x = node[ast.node[i].body["base_type"]]
                    node[i].base_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at RangeType::base_type'))
                else:
                    node[i].base_type = None
                if ast.node[i].body["range"] is not None:
                    x = node[ast.node[i].body["range"]]
                    node[i].range = x if isinstance(x,Range) else raiseError(TypeError('type mismatch at RangeType::range'))
                else:
                    node[i].range = None
            case NodeType.ENUM_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at EnumType::is_explicit'))
                x = ast.node[i].body["is_int_set"]
                node[i].is_int_set = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at EnumType::is_int_set'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at EnumType::bit_size'))
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Enum) else raiseError(TypeError('type mismatch at EnumType::base'))
                else:
                    node[i].base = None
            case NodeType.INT_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at IntLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at IntLiteral::value'))
            case NodeType.BOOL_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at BoolLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at BoolLiteral::value'))
            case NodeType.STR_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at StrLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at StrLiteral::value'))
            case NodeType.INPUT:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Input::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
            case NodeType.OUTPUT:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Output::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
            case NodeType.CONFIG:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Config::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
            case NodeType.FIELD:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Field::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at Field::ident'))
                else:
                    node[i].ident = None
                node[i].colon_loc = parse_Loc(ast.node[i].body["colon_loc"])
                if ast.node[i].body["field_type"] is not None:
                    x = node[ast.node[i].body["field_type"]]
                    node[i].field_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Field::field_type'))
                else:
                    node[i].field_type = None
                if ast.node[i].body["raw_arguments"] is not None:
                    x = node[ast.node[i].body["raw_arguments"]]
                    node[i].raw_arguments = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Field::raw_arguments'))
                else:
                    node[i].raw_arguments = None
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at Field::arguments'))) for x in ast.node[i].body["arguments"]]
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
            case NodeType.FORMAT:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Format::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at Format::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["body"] is not None:
                    x = node[ast.node[i].body["body"]]
                    node[i].body = x if isinstance(x,IndentBlock) else raiseError(TypeError('type mismatch at Format::body'))
                else:
                    node[i].body = None
            case NodeType.STATE:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at State::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at State::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["body"] is not None:
                    x = node[ast.node[i].body["body"]]
                    node[i].body = x if isinstance(x,IndentBlock) else raiseError(TypeError('type mismatch at State::body'))
                else:
                    node[i].body = None
            case NodeType.ENUM:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Enum::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at Enum::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["scope"] is not None:
                    node[i].scope = scope[ast.node[i].body["scope"]]
                else:
                    node[i].scope = None
                node[i].colon_loc = parse_Loc(ast.node[i].body["colon_loc"])
                if ast.node[i].body["base_type"] is not None:
                    x = node[ast.node[i].body["base_type"]]
                    node[i].base_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Enum::base_type'))
                else:
                    node[i].base_type = None
                node[i].members = [(node[x] if isinstance(node[x],EnumMember) else raiseError(TypeError('type mismatch at Enum::members'))) for x in ast.node[i].body["members"]]
                if ast.node[i].body["enum_type"] is not None:
                    x = node[ast.node[i].body["enum_type"]]
                    node[i].enum_type = x if isinstance(x,EnumType) else raiseError(TypeError('type mismatch at Enum::enum_type'))
                else:
                    node[i].enum_type = None
            case NodeType.ENUM_MEMBER:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at EnumMember::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at EnumMember::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at EnumMember::expr'))
                else:
                    node[i].expr = None
            case NodeType.FUNCTION:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Function::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at Function::ident'))
                else:
                    node[i].ident = None
                node[i].parameters = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch at Function::parameters'))) for x in ast.node[i].body["parameters"]]
                if ast.node[i].body["return_type"] is not None:
                    x = node[ast.node[i].body["return_type"]]
                    node[i].return_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Function::return_type'))
                else:
                    node[i].return_type = None
                if ast.node[i].body["body"] is not None:
                    x = node[ast.node[i].body["body"]]
                    node[i].body = x if isinstance(x,IndentBlock) else raiseError(TypeError('type mismatch at Function::body'))
                else:
                    node[i].body = None
                if ast.node[i].body["func_type"] is not None:
                    x = node[ast.node[i].body["func_type"]]
                    node[i].func_type = x if isinstance(x,FunctionType) else raiseError(TypeError('type mismatch at Function::func_type'))
                else:
                    node[i].func_type = None
                x = ast.node[i].body["is_cast"]
                node[i].is_cast = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at Function::is_cast'))
                node[i].cast_loc = parse_Loc(ast.node[i].body["cast_loc"])
            case NodeType.BUILTIN_FUNCTION:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at BuiltinFunction::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at BuiltinFunction::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["func_type"] is not None:
                    x = node[ast.node[i].body["func_type"]]
                    node[i].func_type = x if isinstance(x,FunctionType) else raiseError(TypeError('type mismatch at BuiltinFunction::func_type'))
                else:
                    node[i].func_type = None
            case _:
                raise TypeError('unknown node type')
    for i in range(len(ast.scope)):
        if ast.scope[i].prev is not None:
            scope[i].prev = scope[ast.scope[i].prev]
        if ast.scope[i].next is not None:
            scope[i].next = scope[ast.scope[i].next]
        if ast.scope[i].branch is not None:
            scope[i].branch = scope[ast.scope[i].branch]
        scope[i].ident = [(node[x] if isinstance(node[x],Ident) else raiseError(TypeError('type mismatch at Scope::ident'))) for x in ast.scope[i].ident]
        if ast.scope[i].owner is not None:
            scope[i].owner = ast.node[ast.scope[i].owner]
        scope[i].branch_root = ast.scope[i].branch_root
    return node[0]

def walk(node: Node, f: Callable[[Callable,Node],None]) -> None:
    match node:
        case x if isinstance(x,Program):
          if x.struct_type is not None:
              if f(f,x.struct_type) == False:
                  return
          for i in range(len(x.elements)):
              if f(f,x.elements[i]) == False:
                  return
        case x if isinstance(x,Comment):
            pass
        case x if isinstance(x,CommentGroup):
          for i in range(len(x.comments)):
              if f(f,x.comments[i]) == False:
                  return
        case x if isinstance(x,Binary):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.left is not None:
              if f(f,x.left) == False:
                  return
          if x.right is not None:
              if f(f,x.right) == False:
                  return
        case x if isinstance(x,Unary):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Cond):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          if x.then is not None:
              if f(f,x.then) == False:
                  return
          if x.els is not None:
              if f(f,x.els) == False:
                  return
        case x if isinstance(x,Ident):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,Call):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.callee is not None:
              if f(f,x.callee) == False:
                  return
          if x.raw_arguments is not None:
              if f(f,x.raw_arguments) == False:
                  return
          for i in range(len(x.arguments)):
              if f(f,x.arguments[i]) == False:
                  return
        case x if isinstance(x,If):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          if x.then is not None:
              if f(f,x.then) == False:
                  return
          if x.els is not None:
              if f(f,x.els) == False:
                  return
        case x if isinstance(x,MemberAccess):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.target is not None:
              if f(f,x.target) == False:
                  return
          if x.member is not None:
              if f(f,x.member) == False:
                  return
        case x if isinstance(x,Paren):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Index):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
          if x.index is not None:
              if f(f,x.index) == False:
                  return
        case x if isinstance(x,Match):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          for i in range(len(x.branch)):
              if f(f,x.branch[i]) == False:
                  return
        case x if isinstance(x,Range):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.start is not None:
              if f(f,x.start) == False:
                  return
          if x.end is not None:
              if f(f,x.end) == False:
                  return
        case x if isinstance(x,TmpVar):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,BlockExpr):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          for i in range(len(x.calls)):
              if f(f,x.calls[i]) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Import):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          if x.import_desc is not None:
              if f(f,x.import_desc) == False:
                  return
        case x if isinstance(x,Cast):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Loop):
          if x.init is not None:
              if f(f,x.init) == False:
                  return
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          if x.step is not None:
              if f(f,x.step) == False:
                  return
          if x.body is not None:
              if f(f,x.body) == False:
                  return
        case x if isinstance(x,IndentBlock):
          if x.struct_type is not None:
              if f(f,x.struct_type) == False:
                  return
          for i in range(len(x.elements)):
              if f(f,x.elements[i]) == False:
                  return
        case x if isinstance(x,MatchBranch):
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          if x.then is not None:
              if f(f,x.then) == False:
                  return
        case x if isinstance(x,UnionCandidate):
            pass
        case x if isinstance(x,Return):
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Break):
            pass
        case x if isinstance(x,Continue):
            pass
        case x if isinstance(x,Assert):
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
        case x if isinstance(x,ImplicitYield):
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,IntType):
            pass
        case x if isinstance(x,IdentType):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
        case x if isinstance(x,IntLiteralType):
            pass
        case x if isinstance(x,StrLiteralType):
            pass
        case x if isinstance(x,VoidType):
            pass
        case x if isinstance(x,BoolType):
            pass
        case x if isinstance(x,ArrayType):
          if x.base_type is not None:
              if f(f,x.base_type) == False:
                  return
          if x.length is not None:
              if f(f,x.length) == False:
                  return
        case x if isinstance(x,FunctionType):
          if x.return_type is not None:
              if f(f,x.return_type) == False:
                  return
          for i in range(len(x.parameters)):
              if f(f,x.parameters[i]) == False:
                  return
        case x if isinstance(x,StructType):
          for i in range(len(x.fields)):
              if f(f,x.fields[i]) == False:
                  return
        case x if isinstance(x,StructUnionType):
          for i in range(len(x.fields)):
              if f(f,x.fields[i]) == False:
                  return
        case x if isinstance(x,UnionType):
          for i in range(len(x.candidates)):
              if f(f,x.candidates[i]) == False:
                  return
          if x.common_type is not None:
              if f(f,x.common_type) == False:
                  return
        case x if isinstance(x,RangeType):
          if x.base_type is not None:
              if f(f,x.base_type) == False:
                  return
        case x if isinstance(x,EnumType):
            pass
        case x if isinstance(x,IntLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,BoolLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,StrLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,Input):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,Output):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,Config):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,Field):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.field_type is not None:
              if f(f,x.field_type) == False:
                  return
          if x.raw_arguments is not None:
              if f(f,x.raw_arguments) == False:
                  return
          for i in range(len(x.arguments)):
              if f(f,x.arguments[i]) == False:
                  return
        case x if isinstance(x,Format):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.body is not None:
              if f(f,x.body) == False:
                  return
        case x if isinstance(x,State):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.body is not None:
              if f(f,x.body) == False:
                  return
        case x if isinstance(x,Enum):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.base_type is not None:
              if f(f,x.base_type) == False:
                  return
          for i in range(len(x.members)):
              if f(f,x.members[i]) == False:
                  return
          if x.enum_type is not None:
              if f(f,x.enum_type) == False:
                  return
        case x if isinstance(x,EnumMember):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,Function):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          for i in range(len(x.parameters)):
              if f(f,x.parameters[i]) == False:
                  return
          if x.return_type is not None:
              if f(f,x.return_type) == False:
                  return
          if x.body is not None:
              if f(f,x.body) == False:
                  return
          if x.func_type is not None:
              if f(f,x.func_type) == False:
                  return
        case x if isinstance(x,BuiltinFunction):
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
          if x.func_type is not None:
              if f(f,x.func_type) == False:
                  return
