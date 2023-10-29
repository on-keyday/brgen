from __future__ import annotations

from typing import Optional,List,Dict,Any,Callable

from enum import Enum

class NodeType(Enum):
    NODE = "node"
    PROGRAM = "program"
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
    LITERAL = "literal"
    INT_LITERAL = "int_literal"
    BOOL_LITERAL = "bool_literal"
    STR_LITERAL = "str_literal"
    INPUT = "input"
    OUTPUT = "output"
    CONFIG = "config"
    STMT = "stmt"
    LOOP = "loop"
    INDENT_BLOCK = "indent_block"
    MATCH_BRANCH = "match_branch"
    RETURN = "return"
    BREAK = "break"
    CONTINUE = "continue"
    ASSERT = "assert"
    IMPLICIT_YIELD = "implicit_yield"
    MEMBER = "member"
    FIELD = "field"
    FORMAT = "format"
    FUNCTION = "function"
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
    UNION_TYPE = "union_type"
    CAST = "cast"
    COMMENT = "comment"
    COMMENT_GROUP = "comment_group"
    UNION_FIELD = "union_field"
    UNION_CANDIDATE = "union_candidate"


class Node:
    loc: Loc


class Expr(Node):
    expr_type: Optional[Type]


class Literal(Expr):
    pass


class Stmt(Node):
    pass


class Member(Stmt):
    belong: Optional[Member]
    ident: Optional[Ident]


class Type(Node):
    is_explicit: bool


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
    cond_scope: Optional[Scope]
    cond: Optional[Expr]
    then: Optional[IndentBlock]
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
    cond_scope: Optional[Scope]
    init: Optional[Expr]
    cond: Optional[Expr]
    step: Optional[Expr]
    body: Optional[IndentBlock]


class IndentBlock(Stmt):
    elements: List[Node]
    scope: Optional[Scope]
    struct_type: Optional[StructType]


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
    colon_loc: Loc
    field_type: Optional[Type]
    raw_arguments: Optional[Expr]
    arguments: List[Expr]


class Format(Member):
    is_enum: bool
    body: Optional[IndentBlock]


class Function(Member):
    parameters: List[Field]
    return_type: Optional[Type]
    body: Optional[IndentBlock]
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
    base: Optional[Expr]
    union_fields: List[UnionField]


class Cast(Expr):
    base: Optional[Call]
    expr: Optional[Expr]


class Comment(Node):
    comment: str


class CommentGroup(Node):
    comments: List[Comment]


class UnionField(Member):
    candidate: List[UnionCandidate]
    union_type: Optional[UnionType]


class UnionCandidate(Stmt):
    cond: Optional[Expr]
    field: Optional[Member]


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
    is_global: bool


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



class RawNode:
    node_type: NodeType
    loc: Loc
    body: Dict[str,Any]

def parse_RawNode(json: dict) -> RawNode:
    ret = RawNode()
    ret.node_type = NodeType(json['node_type'])
    ret.loc = parse_Loc(json['loc'])
    ret.body = json['body']
    return ret

class RawScope:
    prev: Optional[int]
    next: Optional[int]
    branch: Optional[int]
    ident: List[int]
    is_global: bool

def parse_RawScope(json: dict) -> RawScope:
    ret = RawScope()
    ret.prev = json['prev']
    ret.next = json['next']
    ret.branch = json['branch']
    ret.ident = json['ident']
    ret.is_global = json['is_global']
    return ret

class Ast:
    node: List[RawNode]
    scope: List[RawScope]

def parse_Ast(json: dict) -> Ast:
    ret = Ast()
    ret.node = [parse_RawNode(x) for x in json['node']]
    ret.scope = [parse_RawScope(x) for x in json['scope']]
    return ret

class AstFile:
    files: List[str]
    ast: Optional[Ast]
    error: Optional[SrcError]

class TokenFile:
    files: List[str]
    tokens: List[Token]
    error: Optional[SrcError]

def parse_AstFile(data: dict) -> AstFile:
    files = data['files']
    error = data['error']
    ast = data['ast']
    if ast is not None:
        ast = parse_Ast(ast)
    if error is not None:
        error = parse_SrcError(error)
    ret = AstFile()
    ret.files = files
    ret.ast = ast
    ret.error = error
    return ret

def parse_TokenFile(data: dict) -> TokenFile:
    files = data['files']
    error = data['error']
    tokens = data['tokens']
    if error is not None:
        error = parse_SrcError(error)
    ret = TokenFile()
    ret.files = files
    ret.tokens = [parse_Token(x) for x in tokens]
    ret.error = error
    return ret

def raiseError(err):
    raise err

def ast2node(ast :Ast) -> Program:
    if not isinstance(ast,Ast):
        raise TypeError('ast must be Ast')
    node = List[Node]()
    for raw in ast.node:
        match raw.node_type:
            case NodeType.PROGRAM:
                node.append(Program())
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
            case NodeType.LOOP:
                node.append(Loop())
            case NodeType.INDENT_BLOCK:
                node.append(IndentBlock())
            case NodeType.MATCH_BRANCH:
                node.append(MatchBranch())
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
            case NodeType.FIELD:
                node.append(Field())
            case NodeType.FORMAT:
                node.append(Format())
            case NodeType.FUNCTION:
                node.append(Function())
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
            case NodeType.UNION_TYPE:
                node.append(UnionType())
            case NodeType.CAST:
                node.append(Cast())
            case NodeType.COMMENT:
                node.append(Comment())
            case NodeType.COMMENT_GROUP:
                node.append(CommentGroup())
            case NodeType.UNION_FIELD:
                node.append(UnionField())
            case NodeType.UNION_CANDIDATE:
                node.append(UnionCandidate())
            case _:
                raise TypeError('unknown node type')
    scope = [Scope() for _ in range(len(ast.scope))]
    for i in range(len(ast.node)):
        node[i].loc = ast.node[i].loc
        match ast.node[i].node_type:
            case NodeType.PROGRAM:
                x = node[ast.node[i].body["struct_type"]]
                node[i].struct_type = x if isinstance(x,StructType) or x is None else raiseError(TypeError('type mismatch'))
                node[i].elements = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["elements"]]
                node[i].global_scope = scope[ast.node[i].body["global_scope"]]
            case NodeType.BINARY:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].op = BinaryOp(ast.node[i].body["op"])
                x = node[ast.node[i].body["left"]]
                node[i].left = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["right"]]
                node[i].right = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.UNARY:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].op = UnaryOp(ast.node[i].body["op"])
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.COND:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["then"]]
                node[i].then = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].els_loc = parse_Loc(ast.node[i].body["els_loc"])
                x = node[ast.node[i].body["els"]]
                node[i].els = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.IDENT:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["ident"]
                node[i].ident = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
                node[i].usage = IdentUsage(ast.node[i].body["usage"])
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,Node) or x is None else raiseError(TypeError('type mismatch'))
                node[i].scope = scope[ast.node[i].body["scope"]]
            case NodeType.CALL:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["callee"]]
                node[i].callee = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["raw_arguments"]]
                node[i].raw_arguments = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["arguments"]]
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.IF:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["then"]]
                node[i].then = x if isinstance(x,IndentBlock) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["els"]]
                node[i].els = x if isinstance(x,Node) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.MEMBER_ACCESS:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["target"]]
                node[i].target = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["member"]
                node[i].member = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
                node[i].member_loc = parse_Loc(ast.node[i].body["member_loc"])
            case NodeType.PAREN:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.INDEX:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["index"]]
                node[i].index = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.MATCH:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].branch = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["branch"]]
            case NodeType.RANGE:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].op = BinaryOp(ast.node[i].body["op"])
                x = node[ast.node[i].body["start"]]
                node[i].start = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["end"]]
                node[i].end = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.TMP_VAR:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["tmp_var"]
                node[i].tmp_var = x if isinstance(x,int)  else raiseError(TypeError('type mismatch'))
            case NodeType.BLOCK_EXPR:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].calls = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["calls"]]
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.IMPORT:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["path"]
                node[i].path = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,Call) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["import_desc"]]
                node[i].import_desc = x if isinstance(x,Program) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.INT_LITERAL:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
            case NodeType.BOOL_LITERAL:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
            case NodeType.STR_LITERAL:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
            case NodeType.INPUT:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.OUTPUT:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.CONFIG:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.LOOP:
                node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                x = node[ast.node[i].body["init"]]
                node[i].init = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["step"]]
                node[i].step = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["body"]]
                node[i].body = x if isinstance(x,IndentBlock) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.INDENT_BLOCK:
                node[i].elements = [(node[x] if isinstance(node[x],Node) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["elements"]]
                node[i].scope = scope[ast.node[i].body["scope"]]
                x = node[ast.node[i].body["struct_type"]]
                node[i].struct_type = x if isinstance(x,StructType) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.MATCH_BRANCH:
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].sym_loc = parse_Loc(ast.node[i].body["sym_loc"])
                x = node[ast.node[i].body["then"]]
                node[i].then = x if isinstance(x,Node) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.RETURN:
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.BREAK:
                pass
            case NodeType.CONTINUE:
                pass
            case NodeType.ASSERT:
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Binary) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.IMPLICIT_YIELD:
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.FIELD:
                x = node[ast.node[i].body["belong"]]
                node[i].belong = x if isinstance(x,Member) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["ident"]]
                node[i].ident = x if isinstance(x,Ident) or x is None else raiseError(TypeError('type mismatch'))
                node[i].colon_loc = parse_Loc(ast.node[i].body["colon_loc"])
                x = node[ast.node[i].body["field_type"]]
                node[i].field_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["raw_arguments"]]
                node[i].raw_arguments = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["arguments"]]
            case NodeType.FORMAT:
                x = node[ast.node[i].body["belong"]]
                node[i].belong = x if isinstance(x,Member) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["ident"]]
                node[i].ident = x if isinstance(x,Ident) or x is None else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["is_enum"]
                node[i].is_enum = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["body"]]
                node[i].body = x if isinstance(x,IndentBlock) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.FUNCTION:
                x = node[ast.node[i].body["belong"]]
                node[i].belong = x if isinstance(x,Member) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["ident"]]
                node[i].ident = x if isinstance(x,Ident) or x is None else raiseError(TypeError('type mismatch'))
                node[i].parameters = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["parameters"]]
                x = node[ast.node[i].body["return_type"]]
                node[i].return_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["body"]]
                node[i].body = x if isinstance(x,IndentBlock) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["func_type"]]
                node[i].func_type = x if isinstance(x,FunctionType) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.INT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = ast.node[i].body["bit_size"]
                node[i].bit_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch'))
                node[i].endian = Endian(ast.node[i].body["endian"])
                x = ast.node[i].body["is_signed"]
                node[i].is_signed = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
            case NodeType.IDENT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["ident"]]
                node[i].ident = x if isinstance(x,Ident) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,Format) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.INT_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,IntLiteral) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.STR_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,StrLiteral) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.VOID_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
            case NodeType.BOOL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
            case NodeType.ARRAY_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
                x = node[ast.node[i].body["base_type"]]
                node[i].base_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["length"]]
                node[i].length = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.FUNCTION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["return_type"]]
                node[i].return_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                node[i].parameters = [(node[x] if isinstance(node[x],Type) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["parameters"]]
            case NodeType.STRUCT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                node[i].fields = [(node[x] if isinstance(node[x],Member) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["fields"]]
            case NodeType.UNION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch'))
                node[i].fields = [(node[x] if isinstance(node[x],StructType) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["fields"]]
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                node[i].union_fields = [(node[x] if isinstance(node[x],UnionField) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["union_fields"]]
            case NodeType.CAST:
                x = node[ast.node[i].body["expr_type"]]
                node[i].expr_type = x if isinstance(x,Type) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["base"]]
                node[i].base = x if isinstance(x,Call) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["expr"]]
                node[i].expr = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.COMMENT:
                x = ast.node[i].body["comment"]
                node[i].comment = x if isinstance(x,str)  else raiseError(TypeError('type mismatch'))
            case NodeType.COMMENT_GROUP:
                node[i].comments = [(node[x] if isinstance(node[x],Comment) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["comments"]]
            case NodeType.UNION_FIELD:
                x = node[ast.node[i].body["belong"]]
                node[i].belong = x if isinstance(x,Member) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["ident"]]
                node[i].ident = x if isinstance(x,Ident) or x is None else raiseError(TypeError('type mismatch'))
                node[i].candidate = [(node[x] if isinstance(node[x],UnionCandidate) else raiseError(TypeError('type mismatch'))) for x in ast.node[i].body["candidate"]]
                x = node[ast.node[i].body["union_type"]]
                node[i].union_type = x if isinstance(x,UnionType) or x is None else raiseError(TypeError('type mismatch'))
            case NodeType.UNION_CANDIDATE:
                x = node[ast.node[i].body["cond"]]
                node[i].cond = x if isinstance(x,Expr) or x is None else raiseError(TypeError('type mismatch'))
                x = node[ast.node[i].body["field"]]
                node[i].field = x if isinstance(x,Member) or x is None else raiseError(TypeError('type mismatch'))
            case _:
                raise TypeError('unknown node type')
    for i in range(len(ast.scope)):
        scope[i].is_global = ast.scope[i].is_global
        if ast.scope[i].next is not None:
            scope[i].next = scope[ast.scope[i].next]
        if ast.scope[i].branch is not None:
            scope[i].branch = scope[ast.scope[i].branch]
        if ast.scope[i].prev is not None:
            scope[i].prev = scope[ast.scope[i].prev]
        scope[i].ident = [node[x] for x in ast.scope[i].ident]
    return Program(node[0])

def walk(node: Node, f: Callable[[Callable,Node],None]) -> None:
    match node:
        case x if isinstance(x,Program):
          if x.struct_type is not None:
              f(f,x.struct_type)
          for i in range(len(x.elements)):
              f(f,x.elements[i])
        case x if isinstance(x,Binary):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.left is not None:
              f(f,x.left)
          if x.right is not None:
              f(f,x.right)
        case x if isinstance(x,Unary):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Cond):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.cond is not None:
              f(f,x.cond)
          if x.then is not None:
              f(f,x.then)
          if x.els is not None:
              f(f,x.els)
        case x if isinstance(x,Ident):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,Call):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.callee is not None:
              f(f,x.callee)
          if x.raw_arguments is not None:
              f(f,x.raw_arguments)
          for i in range(len(x.arguments)):
              f(f,x.arguments[i])
        case x if isinstance(x,If):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.cond is not None:
              f(f,x.cond)
          if x.then is not None:
              f(f,x.then)
          if x.els is not None:
              f(f,x.els)
        case x if isinstance(x,MemberAccess):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.target is not None:
              f(f,x.target)
        case x if isinstance(x,Paren):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Index):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.expr is not None:
              f(f,x.expr)
          if x.index is not None:
              f(f,x.index)
        case x if isinstance(x,Match):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.cond is not None:
              f(f,x.cond)
          for i in range(len(x.branch)):
              f(f,x.branch[i])
        case x if isinstance(x,Range):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.start is not None:
              f(f,x.start)
          if x.end is not None:
              f(f,x.end)
        case x if isinstance(x,TmpVar):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,BlockExpr):
          if x.expr_type is not None:
              f(f,x.expr_type)
          for i in range(len(x.calls)):
              f(f,x.calls[i])
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Import):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.base is not None:
              f(f,x.base)
          if x.import_desc is not None:
              f(f,x.import_desc)
        case x if isinstance(x,IntLiteral):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,BoolLiteral):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,StrLiteral):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,Input):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,Output):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,Config):
          if x.expr_type is not None:
              f(f,x.expr_type)
        case x if isinstance(x,Loop):
          if x.init is not None:
              f(f,x.init)
          if x.cond is not None:
              f(f,x.cond)
          if x.step is not None:
              f(f,x.step)
          if x.body is not None:
              f(f,x.body)
        case x if isinstance(x,IndentBlock):
          for i in range(len(x.elements)):
              f(f,x.elements[i])
          if x.struct_type is not None:
              f(f,x.struct_type)
        case x if isinstance(x,MatchBranch):
          if x.cond is not None:
              f(f,x.cond)
          if x.then is not None:
              f(f,x.then)
        case x if isinstance(x,Return):
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Break):
            pass
        case x if isinstance(x,Continue):
            pass
        case x if isinstance(x,Assert):
          if x.cond is not None:
              f(f,x.cond)
        case x if isinstance(x,ImplicitYield):
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Field):
          if x.ident is not None:
              f(f,x.ident)
          if x.field_type is not None:
              f(f,x.field_type)
          if x.raw_arguments is not None:
              f(f,x.raw_arguments)
          for i in range(len(x.arguments)):
              f(f,x.arguments[i])
        case x if isinstance(x,Format):
          if x.ident is not None:
              f(f,x.ident)
          if x.body is not None:
              f(f,x.body)
        case x if isinstance(x,Function):
          if x.ident is not None:
              f(f,x.ident)
          for i in range(len(x.parameters)):
              f(f,x.parameters[i])
          if x.return_type is not None:
              f(f,x.return_type)
          if x.body is not None:
              f(f,x.body)
          if x.func_type is not None:
              f(f,x.func_type)
        case x if isinstance(x,IntType):
            pass
        case x if isinstance(x,IdentType):
          if x.ident is not None:
              f(f,x.ident)
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
              f(f,x.base_type)
          if x.length is not None:
              f(f,x.length)
        case x if isinstance(x,FunctionType):
          if x.return_type is not None:
              f(f,x.return_type)
          for i in range(len(x.parameters)):
              f(f,x.parameters[i])
        case x if isinstance(x,StructType):
          for i in range(len(x.fields)):
              f(f,x.fields[i])
        case x if isinstance(x,UnionType):
          for i in range(len(x.fields)):
              f(f,x.fields[i])
        case x if isinstance(x,Cast):
          if x.expr_type is not None:
              f(f,x.expr_type)
          if x.base is not None:
              f(f,x.base)
          if x.expr is not None:
              f(f,x.expr)
        case x if isinstance(x,Comment):
            pass
        case x if isinstance(x,CommentGroup):
          for i in range(len(x.comments)):
              f(f,x.comments[i])
        case x if isinstance(x,UnionField):
          if x.ident is not None:
              f(f,x.ident)
          for i in range(len(x.candidate)):
              f(f,x.candidate[i])
        case x if isinstance(x,UnionCandidate):
          if x.cond is not None:
              f(f,x.cond)
          if x.field is not None:
              f(f,x.field)
