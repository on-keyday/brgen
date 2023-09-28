

interface Loc {
    pos: {
        begin: number
        end: number
    }
    file: number
    line: number
    col: number
}

type UnaryOp = "!" | "-";
type BinaryOp = "*" | "/" | "%" | "<<<" | ">>>" | "<<" | ">>" | "&" | "+" | "-" | "|" | "^" | "==" | "!=" | "<" | "<=" | ">" | ">=" | "&&" | "||" | "if" | "else" | ".." | "..=" | "=" | ":=" | "::=" | "+=" | "-=" | "*=" | "/=" | "%=" | "<<=" | ">>=" | "&=" | "|=" | "^=" | ",";


interface Node {
    node_type: string
    loc: Loc
}

interface Scope {
    prev: Scope | null
    next: Scope | null
    branch: Scope | null
    ident: Node[]
}

interface Type extends Node { }


interface Program extends Node {
    struct_type: StructType
    elements: Node[]
    global_scope: Scope
}

interface Expr extends Node {
    expr_type: Type
}

interface Binary extends Expr {
    op: BinaryOp
    left: Expr
    right: Expr
}

interface Unary extends Expr {
    op: UnaryOp
    expr: Expr
}

interface Cond extends Expr {
    cond: Expr
    then: Expr
    els_loc: Loc
    els: Expr
}

type IdentUsage = "unknown" | "reference" | "define_variable" | "define_const" | "define_field" | "define_format" | "define_fn" | "reference_type";

interface Ident extends Expr {
    ident: string
    usage: IdentUsage
    base: Node
    scope: Scope
}

interface Call extends Expr {
    callee: Expr
    raw_arguments: Expr
    arguments: Expr[]
    end_loc: Loc
}

interface If extends Expr {
    cond: Expr
    then: IndentScope
    els: Node
}

interface MemberAccess extends Expr {
    target: Expr
    member: string
    member_loc: Loc
}

interface Paren extends Expr {
    expr: Expr
    end_loc: Loc
}

interface Index extends Expr {
    expr: Expr
    index: Expr
    end_loc: Loc
}

interface Match extends Expr {
    cond: Expr
    branch: MatchBranch[]
    scope: Scope
}

interface Range extends Expr {
    op: BinaryOp
    start: Expr
    end: Expr
}

interface TmpVar extends Expr {
    tmp_var: number
}

interface BlockExpr extends Expr {
    calls: Node[]
    expr: Expr
}

interface Import extends Expr {
    path: string
    base: Call
    import_desc: Program
}

interface Literal extends Expr { }

interface IntLiteral extends Literal {
    value: string
}

interface BoolLiteral extends Literal {
    value: boolean
}

interface StrLiteral extends Literal {
    value: string
}

interface Input extends Literal { }

interface Output extends Literal { }

interface Config extends Literal { }

interface Stmt extends Node { }

interface Loop extends Stmt {
    init: Expr
    cond: Expr
    step: Expr
    body: IndentScope
}

interface IndentScope extends Stmt {
    elements: Node[]
    scope: Scope
}

interface MatchBranch extends Stmt {
    cond: Expr
    sym_loc: Loc
    then: Node
}

interface Return extends Stmt {
    expr: Expr
}

interface Break extends Stmt { }

interface Continue extends Stmt { }

interface Assert extends Stmt {
    cond: Binary
}

interface ImplicitYield extends Stmt {
    expr: Expr
}

interface Member extends Node { }

interface Field extends Member, Stmt {
    ident: Ident
    colon_loc: Loc
    field_type: Type
    raw_arguments: Expr
    arguments: Expr[]
    belong: Format
}

interface Format extends Member, Stmt {
    is_enum: boolean
    ident: Ident
    body: IndentScope
    belong: Format
    struct_type: StructType
}

interface Function extends Member, Stmt {
    ident: Ident
    parameters: Field[]
    return_type: Type
    belong: Format
    body: IndentScope
    func_type: FunctionType
}

interface StructType extends Type {
    fields: Member[]
}

interface UnionType extends Type {
    fields: StructType[]
}

type Endian = "unspec" | "big" | "little";

interface IntType extends Type {
    bit_size: number
    endian: Endian
    is_signed: boolean
}

interface IdentType extends Type {
    ident: Ident
    base: Format
}

interface IntLiteralType extends Type {
    base: IntLiteral
}

interface StrLiteralType extends Type {
    base: StrLiteral
}

interface VoidType extends Type { }

interface BoolType extends Type { }

interface ArrayType extends Type {
    end_loc: Loc
    base_type: Type
    length: Expr
}

interface FunctionType extends Type {
    return_type: Type
    parameters: Type[]
}



const NodeNames = [
    "node",
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
];

const ExprNames = [
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
];

const StmtNames = [
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
];

const LiteralNames = [
    "int_literal",
    "bool_literal",
    "str_literal",
    "input",
    "output",
    "config"
];

const TypeNames = [
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
];

const isNode = (node: any): node is Node => {
    return NodeNames.includes(node?.node_type || "");
}

const isProgram = (node: any): node is Program => {
    return node?.node_type === "program";
}

const isExpr = (node: any): node is Expr => {
    return ExprNames.includes(node?.node_type || "") && node?.expr_type;
}

const isStmt = (node: any): node is Stmt => {
    return StmtNames.includes(node?.node_type || "");
}

const isLiteral = (node: any): node is Literal => {
    return LiteralNames.includes(node?.node_type || "");
}

const isIdent = (node: any): node is Ident => {
    return node?.node_type === "ident";
}

const isType = (node: any): node is Type => {
    return TypeNames.includes(node?.node_type || "");
}

const isIntType = (node: any): node is IntType => {
    return node?.node_type === "int_type";
}

const isIdentType = (node: any): node is IdentType => {
    return node?.node_type === "ident_type";
}

const isIntLiteralType = (node: any): node is IntLiteralType => {
    return node?.node_type === "int_literal_type";
}

const isStrLiteralType = (node: any): node is StrLiteralType => {
    return node?.node_type === "str_literal_type";
}

const isVoidType = (node: any): node is VoidType => {
    return node?.node_type === "void_type";
}

const isBoolType = (node: any): node is BoolType => {
    return node?.node_type === "bool_type";
}

const isArrayType = (node: any): node is ArrayType => {
    return node?.node_type === "array_type";
}

const isFunctionType = (node: any): node is FunctionType => {
    return node?.node_type === "function_type";
}

const isStructType = (node: any): node is StructType => {
    return node?.node_type === "struct_type";
}

const isUnionType = (node: any): node is UnionType => {
    return node?.node_type === "union_type";
}

const isBinary = (node: any): node is Binary => {
    return node?.node_type === "binary";
}

const isUnary = (node: any): node is Unary => {
    return node?.node_type === "unary";
}

const isCond = (node: any): node is Cond => {
    return node?.node_type === "cond";
}

const isCall = (node: any): node is Call => {
    return node?.node_type === "call";
}

const isIf = (node: any): node is If => {
    return node?.node_type === "if";
}

const isMemberAccess = (node: any): node is MemberAccess => {
    return node?.node_type === "member_access";
}

const isParen = (node: any): node is Paren => {
    return node?.node_type === "paren";
}

const isIndex = (node: any): node is Index => {
    return node?.node_type === "index";
}

const isMatch = (node: any): node is Match => {
    return node?.node_type === "match";
}

const isRange = (node: any): node is Range => {
    return node?.node_type === "range";
}

const isTmpVar = (node: any): node is TmpVar => {
    return node?.node_type === "tmp_var";
}

const isBlockExpr = (node: any): node is BlockExpr => {
    return node?.node_type === "block_expr";
}

const isImport = (node: any): node is Import => {
    return node?.node_type === "import";
}

const isLoop = (node: any): node is Loop => {
    return node?.node_type === "loop";
}

const isIndentScope = (node: any): node is IndentScope => {
    return node?.node_type === "indent_scope";
}

const isMatchBranch = (node: any): node is MatchBranch => {
    return node?.node_type === "match_branch";
}

const isReturn = (node: any): node is Return => {
    return node?.node_type === "return";
}

const isBreak = (node: any): node is Break => {
    return node?.node_type === "break";
}

const isContinue = (node: any): node is Continue => {
    return node?.node_type === "continue";
}

const isAssert = (node: any): node is Assert => {
    return node?.node_type === "assert";
}

const isImplicitYield = (node: any): node is ImplicitYield => {
    return node?.node_type === "implicit_yield";
}

const isMember = (node: any): node is Member => {
    return node?.node_type === "member";
}

const isField = (node: any): node is Field => {
    return node?.node_type === "field";
}

const isFormat = (node: any): node is Format => {
    return node?.node_type === "format";
}

const isFunction = (node: any): node is Function => {
    return node?.node_type === "function";
}

const isIntLiteral = (node: any): node is IntLiteral => {
    return node?.node_type === "int_literal";
}

const isBoolLiteral = (node: any): node is BoolLiteral => {
    return node?.node_type === "bool_literal";
}

const isStrLiteral = (node: any): node is StrLiteral => {
    return node?.node_type === "str_literal";
}



const isInput = (node: any): node is Input => {
    return node?.node_type === "input";
}

const isOutput = (node: any): node is Output => {
    return node?.node_type === "output";
}

const isConfig = (node: any): node is Config => {
    return node?.node_type === "config";
}


export {
    Loc,
    UnaryOp,
    BinaryOp,
    Node,
    Scope,
    Type,
    Program,
    Expr,
    Binary,
    Unary,
    Cond,
    Ident,
    Call,
    If,
    MemberAccess,
    Paren,
    Index,
    Match,
    Range,
    TmpVar,
    BlockExpr,
    Import,
    Literal,
    IntLiteral,
    BoolLiteral,
    StrLiteral,
    Input,
    Output,
    Config,
    Stmt,
    Loop,
    IndentScope,
    MatchBranch,
    Return,
    Break,
    Continue,
    Assert,
    ImplicitYield,
    Member,
    Field,
    Format,
    Function,
    StructType,
    UnionType,
    Endian,
    IntType,
    IdentType,
    IntLiteralType,
    StrLiteralType,
    VoidType,
    BoolType,
    ArrayType,
    FunctionType,
    NodeNames,
    ExprNames,
    StmtNames,
    LiteralNames,
    TypeNames,
    isNode,
    isProgram,
    isExpr,
    isStmt,
    isLiteral,
    isIdent,
    isType,
    isIntType,
    isIdentType,
    isIntLiteralType,
    isStrLiteralType,
    isVoidType,
    isBoolType,
    isArrayType,
    isFunctionType,
    isStructType,
    isUnionType,
    isBinary,
    isUnary,
    isCond,
    isCall,
    isIf,
    isMemberAccess,
    isParen,
    isIndex,
    isMatch,
    isRange,
    isTmpVar,
    isBlockExpr,
    isImport,
    isLoop,
    isIndentScope,
    isMatchBranch,
    isReturn,
    isBreak,
    isContinue,
    isAssert,
    isImplicitYield,
    isMember,
    isField,
    isFormat,
    isFunction,
    isIntLiteral,
    isBoolLiteral,
    isStrLiteral,
    isInput,
    isOutput,
    isConfig
}
