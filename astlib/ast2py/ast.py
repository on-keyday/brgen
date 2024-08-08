from __future__ import annotations

from typing import Optional,List,Dict,Any,Callable

from enum import Enum as PyEnum

class NodeType(PyEnum):
    PROGRAM = "program"
    COMMENT = "comment"
    COMMENT_GROUP = "comment_group"
    FIELD_ARGUMENT = "field_argument"
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
    IDENTITY = "identity"
    TMP_VAR = "tmp_var"
    IMPORT = "import"
    CAST = "cast"
    AVAILABLE = "available"
    SPECIFY_ORDER = "specify_order"
    EXPLICIT_ERROR = "explicit_error"
    IO_OPERATION = "io_operation"
    OR_COND = "or_cond"
    BAD_EXPR = "bad_expr"
    STMT = "stmt"
    LOOP = "loop"
    INDENT_BLOCK = "indent_block"
    SCOPED_STATEMENT = "scoped_statement"
    MATCH_BRANCH = "match_branch"
    UNION_CANDIDATE = "union_candidate"
    RETURN = "return"
    BREAK = "break"
    CONTINUE = "continue"
    ASSERT = "assert"
    IMPLICIT_YIELD = "implicit_yield"
    METADATA = "metadata"
    TYPE = "type"
    INT_TYPE = "int_type"
    FLOAT_TYPE = "float_type"
    IDENT_TYPE = "ident_type"
    INT_LITERAL_TYPE = "int_literal_type"
    STR_LITERAL_TYPE = "str_literal_type"
    REGEX_LITERAL_TYPE = "regex_literal_type"
    VOID_TYPE = "void_type"
    BOOL_TYPE = "bool_type"
    ARRAY_TYPE = "array_type"
    FUNCTION_TYPE = "function_type"
    STRUCT_TYPE = "struct_type"
    STRUCT_UNION_TYPE = "struct_union_type"
    UNION_TYPE = "union_type"
    RANGE_TYPE = "range_type"
    ENUM_TYPE = "enum_type"
    META_TYPE = "meta_type"
    OPTIONAL_TYPE = "optional_type"
    GENERIC_TYPE = "generic_type"
    LITERAL = "literal"
    INT_LITERAL = "int_literal"
    BOOL_LITERAL = "bool_literal"
    STR_LITERAL = "str_literal"
    REGEX_LITERAL = "regex_literal"
    CHAR_LITERAL = "char_literal"
    TYPE_LITERAL = "type_literal"
    SPECIAL_LITERAL = "special_literal"
    MEMBER = "member"
    FIELD = "field"
    FORMAT = "format"
    STATE = "state"
    ENUM = "enum"
    ENUM_MEMBER = "enum_member"
    FUNCTION = "function"


class TokenTag(PyEnum):
    INDENT = "indent"
    SPACE = "space"
    LINE = "line"
    PUNCT = "punct"
    INT_LITERAL = "int_literal"
    BOOL_LITERAL = "bool_literal"
    STR_LITERAL = "str_literal"
    REGEX_LITERAL = "regex_literal"
    CHAR_LITERAL = "char_literal"
    KEYWORD = "keyword"
    IDENT = "ident"
    COMMENT = "comment"
    ERROR = "error"
    UNKNOWN = "unknown"


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
    LEFT_LOGICAL_SHIFT_ASSIGN = "<<="
    RIGHT_LOGICAL_SHIFT_ASSIGN = ">>="
    LEFT_ARITHMETIC_SHIFT_ASSIGN = "<<<="
    RIGHT_ARITHMETIC_SHIFT_ASSIGN = ">>>="
    BIT_AND_ASSIGN = "&="
    BIT_OR_ASSIGN = "|="
    BIT_XOR_ASSIGN = "^="
    COMMA = ","
    IN_ASSIGN = "in"


class IdentUsage(PyEnum):
    UNKNOWN = "unknown"
    BAD_IDENT = "bad_ident"
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
    REFERENCE_MEMBER_TYPE = "reference_member_type"
    MAYBE_TYPE = "maybe_type"
    REFERENCE_BUILTIN_FN = "reference_builtin_fn"


class Endian(PyEnum):
    UNSPEC = "unspec"
    BIG = "big"
    LITTLE = "little"


class ConstantLevel(PyEnum):
    UNKNOWN = "unknown"
    CONSTANT = "constant"
    IMMUTABLE_VARIABLE = "immutable_variable"
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


class Follow(PyEnum):
    UNKNOWN = "unknown"
    END = "end"
    FIXED = "fixed"
    CONSTANT = "constant"
    NORMAL = "normal"


class IoMethod(PyEnum):
    UNSPEC = "unspec"
    OUTPUT_PUT = "output_put"
    INPUT_PEEK = "input_peek"
    INPUT_GET = "input_get"
    INPUT_BACKWARD = "input_backward"
    INPUT_OFFSET = "input_offset"
    INPUT_BIT_OFFSET = "input_bit_offset"
    INPUT_REMAIN = "input_remain"
    INPUT_SUBRANGE = "input_subrange"
    CONFIG_ENDIAN_LITTLE = "config_endian_little"
    CONFIG_ENDIAN_BIG = "config_endian_big"
    CONFIG_ENDIAN_NATIVE = "config_endian_native"
    CONFIG_BIT_ORDER_LSB = "config_bit_order_lsb"
    CONFIG_BIT_ORDER_MSB = "config_bit_order_msb"


class SpecialLiteralKind(PyEnum):
    INPUT = "input"
    OUTPUT = "output"
    CONFIG = "config"


class OrderType(PyEnum):
    BYTE = "byte"
    BIT_STREAM = "bit_stream"
    BIT_MAPPING = "bit_mapping"
    BIT_BOTH = "bit_both"


class BlockTrait(PyEnum):
    NONE = 0
    FIXED_PRIMITIVE = 1
    FIXED_FLOAT = 2
    FIXED_ARRAY = 4
    VARIABLE_ARRAY = 8
    STRUCT = 16
    CONDITIONAL = 32
    STATIC_PEEK = 64
    BIT_FIELD = 128
    READ_STATE = 256
    WRITE_STATE = 512
    TERMINAL_PATTERN = 1024
    BIT_STREAM = 2048
    DYNAMIC_ORDER = 4096
    FULL_INPUT = 8192
    BACKWARD_INPUT = 16384
    MAGIC_VALUE = 32768
    ASSERTION = 65536
    EXPLICIT_ERROR = 131072
    PROCEDURAL = 262144
    FOR_LOOP = 524288
    LOCAL_VARIABLE = 1048576
    DESCRIPTION_ONLY = 2097152
    UNCOMMON_SIZE = 4194304
    CONTROL_FLOW_CHANGE = 8388608


class Node:
    loc: Loc


class Expr(Node):
    expr_type: Optional[Type]
    constant_level: ConstantLevel


class Stmt(Node):
    pass


class Type(Node):
    is_explicit: bool
    non_dynamic_allocation: bool
    bit_alignment: BitAlignment
    bit_size: Optional[int]


class Literal(Expr):
    pass


class Member(Stmt):
    belong: Optional[Member]
    belong_struct: Optional[StructType]
    ident: Optional[Ident]


class Program(Node):
    struct_type: Optional[StructType]
    elements: List[Node]
    global_scope: Optional[Scope]
    metadata: List[Metadata]
    endian: Optional[SpecifyOrder]


class Comment(Node):
    comment: str


class CommentGroup(Node):
    comments: List[Comment]


class FieldArgument(Node):
    raw_arguments: Optional[Expr]
    end_loc: Loc
    collected_arguments: List[Expr]
    arguments: List[Expr]
    assigns: List[Binary]
    alignment: Optional[Expr]
    alignment_value: Optional[int]
    sub_byte_length: Optional[Expr]
    sub_byte_begin: Optional[Expr]
    peek: Optional[Expr]
    peek_value: Optional[int]
    type_map: Optional[TypeLiteral]
    metadata: List[Metadata]


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
    struct_union_type: Optional[StructUnionType]
    cond_scope: Optional[Scope]
    cond: Optional[Identity]
    then: Optional[IndentBlock]
    els: Optional[Node]


class MemberAccess(Expr):
    target: Optional[Expr]
    member: Optional[Ident]
    base: Optional[Ident]


class Paren(Expr):
    expr: Optional[Expr]
    end_loc: Loc


class Index(Expr):
    expr: Optional[Expr]
    index: Optional[Expr]
    end_loc: Loc


class Match(Expr):
    struct_union_type: Optional[StructUnionType]
    cond_scope: Optional[Scope]
    cond: Optional[Identity]
    branch: List[MatchBranch]
    trial_match: bool


class Range(Expr):
    op: BinaryOp
    start: Optional[Expr]
    end: Optional[Expr]


class Identity(Expr):
    expr: Optional[Expr]


class TmpVar(Expr):
    tmp_var: int


class Import(Expr):
    path: str
    base: Optional[Call]
    import_desc: Optional[Program]


class Cast(Expr):
    base: Optional[Call]
    arguments: List[Expr]


class Available(Expr):
    base: Optional[Call]
    target: Optional[Expr]


class SpecifyOrder(Expr):
    base: Optional[Binary]
    order_type: OrderType
    order: Optional[Expr]
    order_value: Optional[int]


class ExplicitError(Expr):
    base: Optional[Call]
    message: Optional[StrLiteral]


class IoOperation(Expr):
    base: Optional[Expr]
    method: IoMethod
    arguments: List[Expr]


class OrCond(Expr):
    base: Optional[Binary]
    conds: List[Expr]


class BadExpr(Expr):
    content: str
    bad_expr: Optional[Expr]


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
    metadata: List[Metadata]
    type_map: Optional[TypeLiteral]
    block_traits: BlockTrait


class ScopedStatement(Stmt):
    struct_type: Optional[StructType]
    statement: Optional[Node]
    scope: Optional[Scope]


class MatchBranch(Stmt):
    belong: Optional[Match]
    cond: Optional[Identity]
    sym_loc: Loc
    then: Optional[Node]


class UnionCandidate(Stmt):
    cond: Optional[Expr]
    field: Optional[Field]


class Return(Stmt):
    expr: Optional[Expr]
    related_function: Optional[Function]


class Break(Stmt):
    related_loop: Optional[Loop]


class Continue(Stmt):
    related_loop: Optional[Loop]


class Assert(Stmt):
    cond: Optional[Binary]
    is_io_related: bool


class ImplicitYield(Stmt):
    expr: Optional[Expr]


class Metadata(Stmt):
    base: Optional[Expr]
    name: str
    values: List[Expr]


class IntType(Type):
    endian: Endian
    is_signed: bool
    is_common_supported: bool


class FloatType(Type):
    endian: Endian
    is_common_supported: bool


class IdentType(Type):
    import_ref: Optional[MemberAccess]
    ident: Optional[Ident]
    base: Optional[Type]


class IntLiteralType(Type):
    base: Optional[IntLiteral]


class StrLiteralType(Type):
    base: Optional[StrLiteral]
    strong_ref: Optional[StrLiteral]


class RegexLiteralType(Type):
    base: Optional[RegexLiteral]
    strong_ref: Optional[RegexLiteral]


class VoidType(Type):
    pass


class BoolType(Type):
    pass


class ArrayType(Type):
    end_loc: Loc
    element_type: Optional[Type]
    length: Optional[Expr]
    length_value: Optional[int]
    is_bytes: bool


class FunctionType(Type):
    return_type: Optional[Type]
    parameters: List[Type]


class StructType(Type):
    fields: List[Member]
    base: Optional[Node]
    recursive: bool
    fixed_header_size: int
    fixed_tail_size: int


class StructUnionType(Type):
    cond: Optional[Expr]
    conds: List[Expr]
    structs: List[StructType]
    base: Optional[Expr]
    union_fields: List[Field]
    exhaustive: bool


class UnionType(Type):
    cond: Optional[Expr]
    candidates: List[UnionCandidate]
    base_type: Optional[StructUnionType]
    common_type: Optional[Type]
    member_candidates: List[Field]


class RangeType(Type):
    base_type: Optional[Type]
    range: Optional[Range]


class EnumType(Type):
    base: Optional[Enum]


class MetaType(Type):
    pass


class OptionalType(Type):
    base_type: Optional[Type]


class GenericType(Type):
    belong: Optional[Member]


class IntLiteral(Literal):
    value: str


class BoolLiteral(Literal):
    value: bool


class StrLiteral(Literal):
    value: str
    length: int


class RegexLiteral(Literal):
    value: str


class CharLiteral(Literal):
    value: str
    code: int


class TypeLiteral(Literal):
    type_literal: Optional[Type]
    end_loc: Loc


class SpecialLiteral(Literal):
    kind: SpecialLiteralKind


class Field(Member):
    colon_loc: Loc
    is_state_variable: bool
    field_type: Optional[Type]
    arguments: Optional[FieldArgument]
    offset_bit: Optional[int]
    offset_recent: int
    tail_offset_bit: Optional[int]
    tail_offset_recent: int
    bit_alignment: BitAlignment
    eventual_bit_alignment: BitAlignment
    follow: Follow
    eventual_follow: Follow
    next: Optional[Field]


class Format(Member):
    body: Optional[IndentBlock]
    encode_fn: Optional[Function]
    decode_fn: Optional[Function]
    cast_fns: List[Function]
    depends: List[IdentType]
    state_variables: List[Field]


class State(Member):
    body: Optional[IndentBlock]


class Enum(Member):
    scope: Optional[Scope]
    colon_loc: Loc
    base_type: Optional[Type]
    members: List[EnumMember]
    enum_type: Optional[EnumType]


class EnumMember(Member):
    raw_expr: Optional[Expr]
    value: Optional[Expr]
    str_literal: Optional[StrLiteral]


class Function(Member):
    parameters: List[Field]
    return_type: Optional[Type]
    body: Optional[IndentBlock]
    func_type: Optional[FunctionType]
    is_cast: bool


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
    success: bool
    files: List[str]
    ast: Optional[JsonAst]
    error: Optional[SrcError]

def parse_AstFile(json: dict) -> AstFile:
    ret = AstFile()
    ret.success = bool(json["success"])
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
    success: bool
    files: List[str]
    tokens: Optional[List[Token]]
    error: Optional[SrcError]

def parse_TokenFile(json: dict) -> TokenFile:
    ret = TokenFile()
    ret.success = bool(json["success"])
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



class GenerateMapFile:
    structs: List[str]
    line_map: List[LineMap]

def parse_GenerateMapFile(json: dict) -> GenerateMapFile:
    ret = GenerateMapFile()
    ret.structs = [str(x) for x in json["structs"]]
    ret.line_map = [parse_LineMap(x) for x in json["line_map"]]
    return ret



class LineMap:
    line: int
    loc: Loc

def parse_LineMap(json: dict) -> LineMap:
    ret = LineMap()
    ret.line = int(json["line"])
    ret.loc = parse_Loc(json["loc"])
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
            case NodeType.FIELD_ARGUMENT:
                node.append(FieldArgument())
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
            case NodeType.IDENTITY:
                node.append(Identity())
            case NodeType.TMP_VAR:
                node.append(TmpVar())
            case NodeType.IMPORT:
                node.append(Import())
            case NodeType.CAST:
                node.append(Cast())
            case NodeType.AVAILABLE:
                node.append(Available())
            case NodeType.SPECIFY_ORDER:
                node.append(SpecifyOrder())
            case NodeType.EXPLICIT_ERROR:
                node.append(ExplicitError())
            case NodeType.IO_OPERATION:
                node.append(IoOperation())
            case NodeType.OR_COND:
                node.append(OrCond())
            case NodeType.BAD_EXPR:
                node.append(BadExpr())
            case NodeType.LOOP:
                node.append(Loop())
            case NodeType.INDENT_BLOCK:
                node.append(IndentBlock())
            case NodeType.SCOPED_STATEMENT:
                node.append(ScopedStatement())
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
            case NodeType.METADATA:
                node.append(Metadata())
            case NodeType.INT_TYPE:
                node.append(IntType())
            case NodeType.FLOAT_TYPE:
                node.append(FloatType())
            case NodeType.IDENT_TYPE:
                node.append(IdentType())
            case NodeType.INT_LITERAL_TYPE:
                node.append(IntLiteralType())
            case NodeType.STR_LITERAL_TYPE:
                node.append(StrLiteralType())
            case NodeType.REGEX_LITERAL_TYPE:
                node.append(RegexLiteralType())
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
            case NodeType.META_TYPE:
                node.append(MetaType())
            case NodeType.OPTIONAL_TYPE:
                node.append(OptionalType())
            case NodeType.GENERIC_TYPE:
                node.append(GenericType())
            case NodeType.INT_LITERAL:
                node.append(IntLiteral())
            case NodeType.BOOL_LITERAL:
                node.append(BoolLiteral())
            case NodeType.STR_LITERAL:
                node.append(StrLiteral())
            case NodeType.REGEX_LITERAL:
                node.append(RegexLiteral())
            case NodeType.CHAR_LITERAL:
                node.append(CharLiteral())
            case NodeType.TYPE_LITERAL:
                node.append(TypeLiteral())
            case NodeType.SPECIAL_LITERAL:
                node.append(SpecialLiteral())
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
                node[i].metadata = [(node[x] if isinstance(node[x],Metadata) else raiseError(TypeError('type mismatch at Program::metadata'))) for x in ast.node[i].body["metadata"]]
                if ast.node[i].body["endian"] is not None:
                    x = node[ast.node[i].body["endian"]]
                    node[i].endian = x if isinstance(x,SpecifyOrder) else raiseError(TypeError('type mismatch at Program::endian'))
                else:
                    node[i].endian = None
            case NodeType.COMMENT:
                x = ast.node[i].body["comment"]
                node[i].comment = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at Comment::comment'))
            case NodeType.COMMENT_GROUP:
                node[i].comments = [(node[x] if isinstance(node[x],Comment) else raiseError(TypeError('type mismatch at CommentGroup::comments'))) for x in ast.node[i].body["comments"]]
            case NodeType.FIELD_ARGUMENT:
                if ast.node[i].body["raw_arguments"] is not None:
                    x = node[ast.node[i].body["raw_arguments"]]
                    node[i].raw_arguments = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at FieldArgument::raw_arguments'))
                else:
                    node[i].raw_arguments = None
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
                node[i].collected_arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at FieldArgument::collected_arguments'))) for x in ast.node[i].body["collected_arguments"]]
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at FieldArgument::arguments'))) for x in ast.node[i].body["arguments"]]
                node[i].assigns = [(node[x] if isinstance(node[x],Binary) else raiseError(TypeError('type mismatch at FieldArgument::assigns'))) for x in ast.node[i].body["assigns"]]
                if ast.node[i].body["alignment"] is not None:
                    x = node[ast.node[i].body["alignment"]]
                    node[i].alignment = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at FieldArgument::alignment'))
                else:
                    node[i].alignment = None
                x = ast.node[i].body["alignment_value"]
                if x is not None:
                    node[i].alignment_value = x if isinstance(x,int) else raiseError(TypeError('type mismatch at FieldArgument::alignment_value'))
                else:
                    node[i].alignment_value = None
                if ast.node[i].body["sub_byte_length"] is not None:
                    x = node[ast.node[i].body["sub_byte_length"]]
                    node[i].sub_byte_length = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at FieldArgument::sub_byte_length'))
                else:
                    node[i].sub_byte_length = None
                if ast.node[i].body["sub_byte_begin"] is not None:
                    x = node[ast.node[i].body["sub_byte_begin"]]
                    node[i].sub_byte_begin = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at FieldArgument::sub_byte_begin'))
                else:
                    node[i].sub_byte_begin = None
                if ast.node[i].body["peek"] is not None:
                    x = node[ast.node[i].body["peek"]]
                    node[i].peek = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at FieldArgument::peek'))
                else:
                    node[i].peek = None
                x = ast.node[i].body["peek_value"]
                if x is not None:
                    node[i].peek_value = x if isinstance(x,int) else raiseError(TypeError('type mismatch at FieldArgument::peek_value'))
                else:
                    node[i].peek_value = None
                if ast.node[i].body["type_map"] is not None:
                    x = node[ast.node[i].body["type_map"]]
                    node[i].type_map = x if isinstance(x,TypeLiteral) else raiseError(TypeError('type mismatch at FieldArgument::type_map'))
                else:
                    node[i].type_map = None
                node[i].metadata = [(node[x] if isinstance(node[x],Metadata) else raiseError(TypeError('type mismatch at FieldArgument::metadata'))) for x in ast.node[i].body["metadata"]]
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
                if ast.node[i].body["struct_union_type"] is not None:
                    x = node[ast.node[i].body["struct_union_type"]]
                    node[i].struct_union_type = x if isinstance(x,StructUnionType) else raiseError(TypeError('type mismatch at If::struct_union_type'))
                else:
                    node[i].struct_union_type = None
                if ast.node[i].body["cond_scope"] is not None:
                    node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                else:
                    node[i].cond_scope = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Identity) else raiseError(TypeError('type mismatch at If::cond'))
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
                    node[i].base = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at MemberAccess::base'))
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
                if ast.node[i].body["struct_union_type"] is not None:
                    x = node[ast.node[i].body["struct_union_type"]]
                    node[i].struct_union_type = x if isinstance(x,StructUnionType) else raiseError(TypeError('type mismatch at Match::struct_union_type'))
                else:
                    node[i].struct_union_type = None
                if ast.node[i].body["cond_scope"] is not None:
                    node[i].cond_scope = scope[ast.node[i].body["cond_scope"]]
                else:
                    node[i].cond_scope = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Identity) else raiseError(TypeError('type mismatch at Match::cond'))
                else:
                    node[i].cond = None
                node[i].branch = [(node[x] if isinstance(node[x],MatchBranch) else raiseError(TypeError('type mismatch at Match::branch'))) for x in ast.node[i].body["branch"]]
                x = ast.node[i].body["trial_match"]
                node[i].trial_match = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at Match::trial_match'))
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
            case NodeType.IDENTITY:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Identity::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Identity::expr'))
                else:
                    node[i].expr = None
            case NodeType.TMP_VAR:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at TmpVar::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["tmp_var"]
                node[i].tmp_var = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at TmpVar::tmp_var'))
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
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at Cast::arguments'))) for x in ast.node[i].body["arguments"]]
            case NodeType.AVAILABLE:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Available::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Call) else raiseError(TypeError('type mismatch at Available::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["target"] is not None:
                    x = node[ast.node[i].body["target"]]
                    node[i].target = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Available::target'))
                else:
                    node[i].target = None
            case NodeType.SPECIFY_ORDER:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at SpecifyOrder::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Binary) else raiseError(TypeError('type mismatch at SpecifyOrder::base'))
                else:
                    node[i].base = None
                node[i].order_type = OrderType(ast.node[i].body["order_type"])
                if ast.node[i].body["order"] is not None:
                    x = node[ast.node[i].body["order"]]
                    node[i].order = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at SpecifyOrder::order'))
                else:
                    node[i].order = None
                x = ast.node[i].body["order_value"]
                if x is not None:
                    node[i].order_value = x if isinstance(x,int) else raiseError(TypeError('type mismatch at SpecifyOrder::order_value'))
                else:
                    node[i].order_value = None
            case NodeType.EXPLICIT_ERROR:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at ExplicitError::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Call) else raiseError(TypeError('type mismatch at ExplicitError::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["message"] is not None:
                    x = node[ast.node[i].body["message"]]
                    node[i].message = x if isinstance(x,StrLiteral) else raiseError(TypeError('type mismatch at ExplicitError::message'))
                else:
                    node[i].message = None
            case NodeType.IO_OPERATION:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at IoOperation::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at IoOperation::base'))
                else:
                    node[i].base = None
                node[i].method = IoMethod(ast.node[i].body["method"])
                node[i].arguments = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at IoOperation::arguments'))) for x in ast.node[i].body["arguments"]]
            case NodeType.OR_COND:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at OrCond::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Binary) else raiseError(TypeError('type mismatch at OrCond::base'))
                else:
                    node[i].base = None
                node[i].conds = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at OrCond::conds'))) for x in ast.node[i].body["conds"]]
            case NodeType.BAD_EXPR:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at BadExpr::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["content"]
                node[i].content = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at BadExpr::content'))
                if ast.node[i].body["bad_expr"] is not None:
                    x = node[ast.node[i].body["bad_expr"]]
                    node[i].bad_expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at BadExpr::bad_expr'))
                else:
                    node[i].bad_expr = None
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
                node[i].metadata = [(node[x] if isinstance(node[x],Metadata) else raiseError(TypeError('type mismatch at IndentBlock::metadata'))) for x in ast.node[i].body["metadata"]]
                if ast.node[i].body["type_map"] is not None:
                    x = node[ast.node[i].body["type_map"]]
                    node[i].type_map = x if isinstance(x,TypeLiteral) else raiseError(TypeError('type mismatch at IndentBlock::type_map'))
                else:
                    node[i].type_map = None
                node[i].block_traits = BlockTrait(ast.node[i].body["block_traits"])
            case NodeType.SCOPED_STATEMENT:
                if ast.node[i].body["struct_type"] is not None:
                    x = node[ast.node[i].body["struct_type"]]
                    node[i].struct_type = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at ScopedStatement::struct_type'))
                else:
                    node[i].struct_type = None
                if ast.node[i].body["statement"] is not None:
                    x = node[ast.node[i].body["statement"]]
                    node[i].statement = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at ScopedStatement::statement'))
                else:
                    node[i].statement = None
                if ast.node[i].body["scope"] is not None:
                    node[i].scope = scope[ast.node[i].body["scope"]]
                else:
                    node[i].scope = None
            case NodeType.MATCH_BRANCH:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Match) else raiseError(TypeError('type mismatch at MatchBranch::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Identity) else raiseError(TypeError('type mismatch at MatchBranch::cond'))
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
                if ast.node[i].body["related_function"] is not None:
                    x = node[ast.node[i].body["related_function"]]
                    node[i].related_function = x if isinstance(x,Function) else raiseError(TypeError('type mismatch at Return::related_function'))
                else:
                    node[i].related_function = None
            case NodeType.BREAK:
                if ast.node[i].body["related_loop"] is not None:
                    x = node[ast.node[i].body["related_loop"]]
                    node[i].related_loop = x if isinstance(x,Loop) else raiseError(TypeError('type mismatch at Break::related_loop'))
                else:
                    node[i].related_loop = None
            case NodeType.CONTINUE:
                if ast.node[i].body["related_loop"] is not None:
                    x = node[ast.node[i].body["related_loop"]]
                    node[i].related_loop = x if isinstance(x,Loop) else raiseError(TypeError('type mismatch at Continue::related_loop'))
                else:
                    node[i].related_loop = None
            case NodeType.ASSERT:
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Binary) else raiseError(TypeError('type mismatch at Assert::cond'))
                else:
                    node[i].cond = None
                x = ast.node[i].body["is_io_related"]
                node[i].is_io_related = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at Assert::is_io_related'))
            case NodeType.IMPLICIT_YIELD:
                if ast.node[i].body["expr"] is not None:
                    x = node[ast.node[i].body["expr"]]
                    node[i].expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at ImplicitYield::expr'))
                else:
                    node[i].expr = None
            case NodeType.METADATA:
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at Metadata::base'))
                else:
                    node[i].base = None
                x = ast.node[i].body["name"]
                node[i].name = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at Metadata::name'))
                node[i].values = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at Metadata::values'))) for x in ast.node[i].body["values"]]
            case NodeType.INT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at IntType::bit_size'))
                else:
                    node[i].bit_size = None
                node[i].endian = Endian(ast.node[i].body["endian"])
                x = ast.node[i].body["is_signed"]
                node[i].is_signed = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_signed'))
                x = ast.node[i].body["is_common_supported"]
                node[i].is_common_supported = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntType::is_common_supported'))
            case NodeType.FLOAT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FloatType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FloatType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at FloatType::bit_size'))
                else:
                    node[i].bit_size = None
                node[i].endian = Endian(ast.node[i].body["endian"])
                x = ast.node[i].body["is_common_supported"]
                node[i].is_common_supported = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FloatType::is_common_supported'))
            case NodeType.IDENT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IdentType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IdentType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at IdentType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["import_ref"] is not None:
                    x = node[ast.node[i].body["import_ref"]]
                    node[i].import_ref = x if isinstance(x,MemberAccess) else raiseError(TypeError('type mismatch at IdentType::import_ref'))
                else:
                    node[i].import_ref = None
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
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at IntLiteralType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at IntLiteralType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,IntLiteral) else raiseError(TypeError('type mismatch at IntLiteralType::base'))
                else:
                    node[i].base = None
            case NodeType.STR_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StrLiteralType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StrLiteralType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at StrLiteralType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,StrLiteral) else raiseError(TypeError('type mismatch at StrLiteralType::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["strong_ref"] is not None:
                    x = node[ast.node[i].body["strong_ref"]]
                    node[i].strong_ref = x if isinstance(x,StrLiteral) else raiseError(TypeError('type mismatch at StrLiteralType::strong_ref'))
                else:
                    node[i].strong_ref = None
            case NodeType.REGEX_LITERAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RegexLiteralType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RegexLiteralType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at RegexLiteralType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,RegexLiteral) else raiseError(TypeError('type mismatch at RegexLiteralType::base'))
                else:
                    node[i].base = None
                if ast.node[i].body["strong_ref"] is not None:
                    x = node[ast.node[i].body["strong_ref"]]
                    node[i].strong_ref = x if isinstance(x,RegexLiteral) else raiseError(TypeError('type mismatch at RegexLiteralType::strong_ref'))
                else:
                    node[i].strong_ref = None
            case NodeType.VOID_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at VoidType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at VoidType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at VoidType::bit_size'))
                else:
                    node[i].bit_size = None
            case NodeType.BOOL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at BoolType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at BoolType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at BoolType::bit_size'))
                else:
                    node[i].bit_size = None
            case NodeType.ARRAY_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at ArrayType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at ArrayType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at ArrayType::bit_size'))
                else:
                    node[i].bit_size = None
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
                if ast.node[i].body["element_type"] is not None:
                    x = node[ast.node[i].body["element_type"]]
                    node[i].element_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at ArrayType::element_type'))
                else:
                    node[i].element_type = None
                if ast.node[i].body["length"] is not None:
                    x = node[ast.node[i].body["length"]]
                    node[i].length = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at ArrayType::length'))
                else:
                    node[i].length = None
                x = ast.node[i].body["length_value"]
                if x is not None:
                    node[i].length_value = x if isinstance(x,int) else raiseError(TypeError('type mismatch at ArrayType::length_value'))
                else:
                    node[i].length_value = None
                x = ast.node[i].body["is_bytes"]
                node[i].is_bytes = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at ArrayType::is_bytes'))
            case NodeType.FUNCTION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FunctionType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at FunctionType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at FunctionType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["return_type"] is not None:
                    x = node[ast.node[i].body["return_type"]]
                    node[i].return_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at FunctionType::return_type'))
                else:
                    node[i].return_type = None
                node[i].parameters = [(node[x] if isinstance(node[x],Type) else raiseError(TypeError('type mismatch at FunctionType::parameters'))) for x in ast.node[i].body["parameters"]]
            case NodeType.STRUCT_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at StructType::bit_size'))
                else:
                    node[i].bit_size = None
                node[i].fields = [(node[x] if isinstance(node[x],Member) else raiseError(TypeError('type mismatch at StructType::fields'))) for x in ast.node[i].body["fields"]]
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Node) else raiseError(TypeError('type mismatch at StructType::base'))
                else:
                    node[i].base = None
                x = ast.node[i].body["recursive"]
                node[i].recursive = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructType::recursive'))
                x = ast.node[i].body["fixed_header_size"]
                node[i].fixed_header_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StructType::fixed_header_size'))
                x = ast.node[i].body["fixed_tail_size"]
                node[i].fixed_tail_size = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StructType::fixed_tail_size'))
            case NodeType.STRUCT_UNION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructUnionType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructUnionType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at StructUnionType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["cond"] is not None:
                    x = node[ast.node[i].body["cond"]]
                    node[i].cond = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at StructUnionType::cond'))
                else:
                    node[i].cond = None
                node[i].conds = [(node[x] if isinstance(node[x],Expr) else raiseError(TypeError('type mismatch at StructUnionType::conds'))) for x in ast.node[i].body["conds"]]
                node[i].structs = [(node[x] if isinstance(node[x],StructType) else raiseError(TypeError('type mismatch at StructUnionType::structs'))) for x in ast.node[i].body["structs"]]
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at StructUnionType::base'))
                else:
                    node[i].base = None
                node[i].union_fields = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch at StructUnionType::union_fields'))) for x in ast.node[i].body["union_fields"]]
                x = ast.node[i].body["exhaustive"]
                node[i].exhaustive = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at StructUnionType::exhaustive'))
            case NodeType.UNION_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at UnionType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at UnionType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at UnionType::bit_size'))
                else:
                    node[i].bit_size = None
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
                node[i].member_candidates = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch at UnionType::member_candidates'))) for x in ast.node[i].body["member_candidates"]]
            case NodeType.RANGE_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RangeType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at RangeType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at RangeType::bit_size'))
                else:
                    node[i].bit_size = None
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
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at EnumType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at EnumType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["base"] is not None:
                    x = node[ast.node[i].body["base"]]
                    node[i].base = x if isinstance(x,Enum) else raiseError(TypeError('type mismatch at EnumType::base'))
                else:
                    node[i].base = None
            case NodeType.META_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at MetaType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at MetaType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at MetaType::bit_size'))
                else:
                    node[i].bit_size = None
            case NodeType.OPTIONAL_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at OptionalType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at OptionalType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at OptionalType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["base_type"] is not None:
                    x = node[ast.node[i].body["base_type"]]
                    node[i].base_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at OptionalType::base_type'))
                else:
                    node[i].base_type = None
            case NodeType.GENERIC_TYPE:
                x = ast.node[i].body["is_explicit"]
                node[i].is_explicit = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at GenericType::is_explicit'))
                x = ast.node[i].body["non_dynamic_allocation"]
                node[i].non_dynamic_allocation = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at GenericType::non_dynamic_allocation'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                x = ast.node[i].body["bit_size"]
                if x is not None:
                    node[i].bit_size = x if isinstance(x,int) else raiseError(TypeError('type mismatch at GenericType::bit_size'))
                else:
                    node[i].bit_size = None
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at GenericType::belong'))
                else:
                    node[i].belong = None
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
                x = ast.node[i].body["length"]
                node[i].length = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at StrLiteral::length'))
            case NodeType.REGEX_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at RegexLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at RegexLiteral::value'))
            case NodeType.CHAR_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at CharLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                x = ast.node[i].body["value"]
                node[i].value = x if isinstance(x,str)  else raiseError(TypeError('type mismatch at CharLiteral::value'))
                x = ast.node[i].body["code"]
                node[i].code = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at CharLiteral::code'))
            case NodeType.TYPE_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at TypeLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                if ast.node[i].body["type_literal"] is not None:
                    x = node[ast.node[i].body["type_literal"]]
                    node[i].type_literal = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at TypeLiteral::type_literal'))
                else:
                    node[i].type_literal = None
                node[i].end_loc = parse_Loc(ast.node[i].body["end_loc"])
            case NodeType.SPECIAL_LITERAL:
                if ast.node[i].body["expr_type"] is not None:
                    x = node[ast.node[i].body["expr_type"]]
                    node[i].expr_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at SpecialLiteral::expr_type'))
                else:
                    node[i].expr_type = None
                node[i].constant_level = ConstantLevel(ast.node[i].body["constant_level"])
                node[i].kind = SpecialLiteralKind(ast.node[i].body["kind"])
            case NodeType.FIELD:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Field::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at Field::belong_struct'))
                else:
                    node[i].belong_struct = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at Field::ident'))
                else:
                    node[i].ident = None
                node[i].colon_loc = parse_Loc(ast.node[i].body["colon_loc"])
                x = ast.node[i].body["is_state_variable"]
                node[i].is_state_variable = x if isinstance(x,bool)  else raiseError(TypeError('type mismatch at Field::is_state_variable'))
                if ast.node[i].body["field_type"] is not None:
                    x = node[ast.node[i].body["field_type"]]
                    node[i].field_type = x if isinstance(x,Type) else raiseError(TypeError('type mismatch at Field::field_type'))
                else:
                    node[i].field_type = None
                if ast.node[i].body["arguments"] is not None:
                    x = node[ast.node[i].body["arguments"]]
                    node[i].arguments = x if isinstance(x,FieldArgument) else raiseError(TypeError('type mismatch at Field::arguments'))
                else:
                    node[i].arguments = None
                x = ast.node[i].body["offset_bit"]
                if x is not None:
                    node[i].offset_bit = x if isinstance(x,int) else raiseError(TypeError('type mismatch at Field::offset_bit'))
                else:
                    node[i].offset_bit = None
                x = ast.node[i].body["offset_recent"]
                node[i].offset_recent = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at Field::offset_recent'))
                x = ast.node[i].body["tail_offset_bit"]
                if x is not None:
                    node[i].tail_offset_bit = x if isinstance(x,int) else raiseError(TypeError('type mismatch at Field::tail_offset_bit'))
                else:
                    node[i].tail_offset_bit = None
                x = ast.node[i].body["tail_offset_recent"]
                node[i].tail_offset_recent = x if isinstance(x,int)  else raiseError(TypeError('type mismatch at Field::tail_offset_recent'))
                node[i].bit_alignment = BitAlignment(ast.node[i].body["bit_alignment"])
                node[i].eventual_bit_alignment = BitAlignment(ast.node[i].body["eventual_bit_alignment"])
                node[i].follow = Follow(ast.node[i].body["follow"])
                node[i].eventual_follow = Follow(ast.node[i].body["eventual_follow"])
                if ast.node[i].body["next"] is not None:
                    x = node[ast.node[i].body["next"]]
                    node[i].next = x if isinstance(x,Field) else raiseError(TypeError('type mismatch at Field::next'))
                else:
                    node[i].next = None
            case NodeType.FORMAT:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Format::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at Format::belong_struct'))
                else:
                    node[i].belong_struct = None
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
                if ast.node[i].body["encode_fn"] is not None:
                    x = node[ast.node[i].body["encode_fn"]]
                    node[i].encode_fn = x if isinstance(x,Function) else raiseError(TypeError('type mismatch at Format::encode_fn'))
                else:
                    node[i].encode_fn = None
                if ast.node[i].body["decode_fn"] is not None:
                    x = node[ast.node[i].body["decode_fn"]]
                    node[i].decode_fn = x if isinstance(x,Function) else raiseError(TypeError('type mismatch at Format::decode_fn'))
                else:
                    node[i].decode_fn = None
                node[i].cast_fns = [(node[x] if isinstance(node[x],Function) else raiseError(TypeError('type mismatch at Format::cast_fns'))) for x in ast.node[i].body["cast_fns"]]
                node[i].depends = [(node[x] if isinstance(node[x],IdentType) else raiseError(TypeError('type mismatch at Format::depends'))) for x in ast.node[i].body["depends"]]
                node[i].state_variables = [(node[x] if isinstance(node[x],Field) else raiseError(TypeError('type mismatch at Format::state_variables'))) for x in ast.node[i].body["state_variables"]]
            case NodeType.STATE:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at State::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at State::belong_struct'))
                else:
                    node[i].belong_struct = None
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
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at Enum::belong_struct'))
                else:
                    node[i].belong_struct = None
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
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at EnumMember::belong_struct'))
                else:
                    node[i].belong_struct = None
                if ast.node[i].body["ident"] is not None:
                    x = node[ast.node[i].body["ident"]]
                    node[i].ident = x if isinstance(x,Ident) else raiseError(TypeError('type mismatch at EnumMember::ident'))
                else:
                    node[i].ident = None
                if ast.node[i].body["raw_expr"] is not None:
                    x = node[ast.node[i].body["raw_expr"]]
                    node[i].raw_expr = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at EnumMember::raw_expr'))
                else:
                    node[i].raw_expr = None
                if ast.node[i].body["value"] is not None:
                    x = node[ast.node[i].body["value"]]
                    node[i].value = x if isinstance(x,Expr) else raiseError(TypeError('type mismatch at EnumMember::value'))
                else:
                    node[i].value = None
                if ast.node[i].body["str_literal"] is not None:
                    x = node[ast.node[i].body["str_literal"]]
                    node[i].str_literal = x if isinstance(x,StrLiteral) else raiseError(TypeError('type mismatch at EnumMember::str_literal'))
                else:
                    node[i].str_literal = None
            case NodeType.FUNCTION:
                if ast.node[i].body["belong"] is not None:
                    x = node[ast.node[i].body["belong"]]
                    node[i].belong = x if isinstance(x,Member) else raiseError(TypeError('type mismatch at Function::belong'))
                else:
                    node[i].belong = None
                if ast.node[i].body["belong_struct"] is not None:
                    x = node[ast.node[i].body["belong_struct"]]
                    node[i].belong_struct = x if isinstance(x,StructType) else raiseError(TypeError('type mismatch at Function::belong_struct'))
                else:
                    node[i].belong_struct = None
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
        case x if isinstance(x,FieldArgument):
          if x.raw_arguments is not None:
              if f(f,x.raw_arguments) == False:
                  return
          for i in range(len(x.arguments)):
              if f(f,x.arguments[i]) == False:
                  return
          for i in range(len(x.assigns)):
              if f(f,x.assigns[i]) == False:
                  return
          if x.alignment is not None:
              if f(f,x.alignment) == False:
                  return
          if x.sub_byte_length is not None:
              if f(f,x.sub_byte_length) == False:
                  return
          if x.sub_byte_begin is not None:
              if f(f,x.sub_byte_begin) == False:
                  return
          if x.peek is not None:
              if f(f,x.peek) == False:
                  return
          if x.type_map is not None:
              if f(f,x.type_map) == False:
                  return
          for i in range(len(x.metadata)):
              if f(f,x.metadata[i]) == False:
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
          if x.struct_union_type is not None:
              if f(f,x.struct_union_type) == False:
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
          if x.struct_union_type is not None:
              if f(f,x.struct_union_type) == False:
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
        case x if isinstance(x,Identity):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.expr is not None:
              if f(f,x.expr) == False:
                  return
        case x if isinstance(x,TmpVar):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
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
          for i in range(len(x.arguments)):
              if f(f,x.arguments[i]) == False:
                  return
        case x if isinstance(x,Available):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          if x.target is not None:
              if f(f,x.target) == False:
                  return
        case x if isinstance(x,SpecifyOrder):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          if x.order is not None:
              if f(f,x.order) == False:
                  return
        case x if isinstance(x,ExplicitError):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          if x.message is not None:
              if f(f,x.message) == False:
                  return
        case x if isinstance(x,IoOperation):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          for i in range(len(x.arguments)):
              if f(f,x.arguments[i]) == False:
                  return
        case x if isinstance(x,OrCond):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          for i in range(len(x.conds)):
              if f(f,x.conds[i]) == False:
                  return
        case x if isinstance(x,BadExpr):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.bad_expr is not None:
              if f(f,x.bad_expr) == False:
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
          if x.type_map is not None:
              if f(f,x.type_map) == False:
                  return
        case x if isinstance(x,ScopedStatement):
          if x.struct_type is not None:
              if f(f,x.struct_type) == False:
                  return
          if x.statement is not None:
              if f(f,x.statement) == False:
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
        case x if isinstance(x,Metadata):
          if x.base is not None:
              if f(f,x.base) == False:
                  return
          for i in range(len(x.values)):
              if f(f,x.values[i]) == False:
                  return
        case x if isinstance(x,IntType):
            pass
        case x if isinstance(x,FloatType):
            pass
        case x if isinstance(x,IdentType):
          if x.import_ref is not None:
              if f(f,x.import_ref) == False:
                  return
          if x.ident is not None:
              if f(f,x.ident) == False:
                  return
        case x if isinstance(x,IntLiteralType):
            pass
        case x if isinstance(x,StrLiteralType):
          if x.strong_ref is not None:
              if f(f,x.strong_ref) == False:
                  return
        case x if isinstance(x,RegexLiteralType):
          if x.strong_ref is not None:
              if f(f,x.strong_ref) == False:
                  return
        case x if isinstance(x,VoidType):
            pass
        case x if isinstance(x,BoolType):
            pass
        case x if isinstance(x,ArrayType):
          if x.element_type is not None:
              if f(f,x.element_type) == False:
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
          if x.cond is not None:
              if f(f,x.cond) == False:
                  return
          for i in range(len(x.conds)):
              if f(f,x.conds[i]) == False:
                  return
          for i in range(len(x.structs)):
              if f(f,x.structs[i]) == False:
                  return
        case x if isinstance(x,UnionType):
          for i in range(len(x.candidates)):
              if f(f,x.candidates[i]) == False:
                  return
          if x.common_type is not None:
              if f(f,x.common_type) == False:
                  return
          for i in range(len(x.member_candidates)):
              if f(f,x.member_candidates[i]) == False:
                  return
        case x if isinstance(x,RangeType):
          if x.base_type is not None:
              if f(f,x.base_type) == False:
                  return
        case x if isinstance(x,EnumType):
            pass
        case x if isinstance(x,MetaType):
            pass
        case x if isinstance(x,OptionalType):
          if x.base_type is not None:
              if f(f,x.base_type) == False:
                  return
        case x if isinstance(x,GenericType):
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
        case x if isinstance(x,RegexLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,CharLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
        case x if isinstance(x,TypeLiteral):
          if x.expr_type is not None:
              if f(f,x.expr_type) == False:
                  return
          if x.type_literal is not None:
              if f(f,x.type_literal) == False:
                  return
        case x if isinstance(x,SpecialLiteral):
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
          if x.arguments is not None:
              if f(f,x.arguments) == False:
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
          if x.raw_expr is not None:
              if f(f,x.raw_expr) == False:
                  return
          if x.value is not None:
              if f(f,x.value) == False:
                  return
          if x.str_literal is not None:
              if f(f,x.str_literal) == False:
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
