use serde_derive::{Deserialize, Serialize};
use std::cell::RefCell;
use std::rc::{Rc, Weak};

/*
{"node": [{"node_type": "node","one_of": ["program","expr","binary","unary","cond","ident","call","if","member_access","paren","index","match","range","tmp_var","block_expr","import","literal","int_literal","bool_literal","str_literal","input","output","config","stmt","loop","indent_scope","match_branch","return","break","continue","assert","implicit_yield","member","field","format","function","type","int_type","ident_type","int_literal_type","str_literal_type","void_type","bool_type","array_type","function_type","struct_type","union_type"]},{"node_type": "program","loc": "loc","struct_type": "shared_ptr<struct_type>","elements": "array<shared_ptr<node>>","global_scope": "shared_ptr<scope>"},{"node_type": "expr","one_of": ["binary","unary","cond","ident","call","if","member_access","paren","index","match","range","tmp_var","block_expr","import","literal","int_literal","bool_literal","str_literal","input","output","config"]},{"node_type": "binary","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["*","/","%","<<",">>","&","+","-","|","^","==","!=","<","<=",">",">=","&&","||","if","else","..","..=","=",":=","::=","+=","-=","*=","/=","%=","<<=",">>=","&=","|=","^=",","],"left": "shared_ptr<expr>","right": "shared_ptr<expr>"},{"node_type": "unary","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["!","-"],"expr": "shared_ptr<expr>"},{"node_type": "cond","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","then": "shared_ptr<expr>","els_loc": "loc","els": "shared_ptr<expr>"},{"node_type": "ident","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","ident": "string","usage": ["unknown","reference","define_variable","define_const","define_field","define_format","define_fn","reference_type"],"base": "weak_ptr<node>","scope": "shared_ptr<scope>"},{"node_type": "call","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","callee": "shared_ptr<expr>","raw_arguments": "shared_ptr<expr>","arguments": "array<shared_ptr<expr>>","end_loc": "loc"},{"node_type": "if","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","then": "shared_ptr<indent_scope>","els": "shared_ptr<node>"},{"node_type": "member_access","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","target": "shared_ptr<expr>","member": "string","member_loc": "loc"},{"node_type": "paren","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","expr": "shared_ptr<expr>","end_loc": "loc"},{"node_type": "index","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","expr": "shared_ptr<expr>","index": "shared_ptr<expr>","end_loc": "loc"},{"node_type": "match","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","cond": "shared_ptr<expr>","branch": "array<shared_ptr<match_branch>>","scope": "shared_ptr<scope>"},{"node_type": "range","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","op": ["*","/","%","<<",">>","&","+","-","|","^","==","!=","<","<=",">",">=","&&","||","if","else","..","..=","=",":=","::=","+=","-=","*=","/=","%=","<<=",">>=","&=","|=","^=",","],"start": "shared_ptr<expr>","end": "shared_ptr<expr>"},{"node_type": "tmp_var","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","tmp_var": "uint"},{"node_type": "block_expr","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","calls": "array<shared_ptr<node>>","expr": "shared_ptr<expr>"},{"node_type": "import","base_node_type": ["expr"],"loc": "loc","expr_type": "shared_ptr<type>","path": "string","base": "shared_ptr<call>","import_desc": "shared_ptr<program>"},{"node_type": "literal","one_of": ["int_literal","bool_literal","str_literal","input","output","config"]},{"node_type": "int_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "string"},{"node_type": "bool_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "bool"},{"node_type": "str_literal","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>","value": "string"},{"node_type": "input","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "output","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "config","base_node_type": ["literal","expr"],"loc": "loc","expr_type": "shared_ptr<type>"},{"node_type": "stmt","one_of": ["loop","indent_scope","match_branch","return","break","continue","assert","implicit_yield","member","field","format","function"]},{"node_type": "loop","base_node_type": ["stmt"],"loc": "loc","init": "shared_ptr<expr>","cond": "shared_ptr<expr>","step": "shared_ptr<expr>","body": "shared_ptr<indent_scope>"},{"node_type": "indent_scope","base_node_type": ["stmt"],"loc": "loc","elements": "array<shared_ptr<node>>","scope": "shared_ptr<scope>"},{"node_type": "match_branch","base_node_type": ["stmt"],"loc": "loc","cond": "shared_ptr<expr>","sym_loc": "loc","then": "shared_ptr<node>"},{"node_type": "return","base_node_type": ["stmt"],"loc": "loc","expr": "shared_ptr<expr>"},{"node_type": "break","base_node_type": ["stmt"],"loc": "loc"},{"node_type": "continue","base_node_type": ["stmt"],"loc": "loc"},{"node_type": "assert","base_node_type": ["stmt"],"loc": "loc","cond": "shared_ptr<binary>"},{"node_type": "implicit_yield","base_node_type": ["stmt"],"loc": "loc","expr": "shared_ptr<expr>"},{"node_type": "member","one_of": ["field","format","function"]},{"node_type": "field","base_node_type": ["member","stmt"],"loc": "loc","ident": "shared_ptr<ident>","colon_loc": "loc","field_type": "shared_ptr<type>","raw_arguments": "shared_ptr<expr>","arguments": "array<shared_ptr<expr>>","belong": "weak_ptr<format>"},{"node_type": "format","base_node_type": ["member","stmt"],"loc": "loc","is_enum": "bool","ident": "shared_ptr<ident>","body": "shared_ptr<indent_scope>","belong": "weak_ptr<format>","struct_type": "shared_ptr<struct_type>"},{"node_type": "function","base_node_type": ["member","stmt"],"loc": "loc","ident": "shared_ptr<ident>","parameters": "array<shared_ptr<field>>","return_type": "shared_ptr<type>","belong": "weak_ptr<format>","body": "shared_ptr<indent_scope>","func_type": "shared_ptr<function_type>"},{"node_type": "type","one_of": ["int_type","ident_type","int_literal_type","str_literal_type","void_type","bool_type","array_type","function_type","struct_type","union_type"]},{"node_type": "int_type","base_node_type": ["type"],"loc": "loc","raw": "string","bit_size": "uint"},{"node_type": "ident_type","base_node_type": ["type"],"loc": "loc","ident": "shared_ptr<ident>","base": "weak_ptr<format>"},{"node_type": "int_literal_type","base_node_type": ["type"],"loc": "loc","base": "weak_ptr<int_literal>"},{"node_type": "str_literal_type","base_node_type": ["type"],"loc": "loc","base": "weak_ptr<str_literal>"},{"node_type": "void_type","base_node_type": ["type"],"loc": "loc"},{"node_type": "bool_type","base_node_type": ["type"],"loc": "loc"},{"node_type": "array_type","base_node_type": ["type"],"loc": "loc","end_loc": "loc","base_type": "shared_ptr<type>","length": "shared_ptr<expr>"},{"node_type": "function_type","base_node_type": ["type"],"loc": "loc","return_type": "shared_ptr<type>","parameters": "array<shared_ptr<type>>"},{"node_type": "struct_type","base_node_type": ["type"],"loc": "loc","fields": "array<shared_ptr<member>>"},{"node_type": "union_type","base_node_type": ["type"],"loc": "loc","fields": "array<shared_ptr<struct_type>>"}],"scope": {"prev": "weak_ptr<scope>","next": "shared_ptr<scope>","branch": "shared_ptr<scope>","ident": "array<std::weak_ptr<node>>"},"loc": {"pos": {"begin": "uint","end": "uint"},"file": "uint"}}
*/

// Define the enum for the "one_of" field
#[derive(Debug)]
pub enum Node {
    Program(Rc<RefCell<Program>>),
    Expr(Rc<RefCell<Expr>>),
    Stmt(Rc<RefCell<Stmt>>),
    Type(Rc<RefCell<Type>>),
}

// Define the Location struct
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Loc {
    pub pos: Pos,
    pub file: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Pos {
    pub begin: u32,
    pub end: u32,
}

// Define the Program struct
#[derive(Debug)]
pub struct Program {
    pub elements: Vec<Option<Rc<Node>>>,
    pub global_scope: Option<Weak<Scope>>,
    pub struct_type: Option<Rc<StructType>>,
}

// Define the Expr enum
#[derive(Debug)]
pub enum Expr {
    Binary(BinaryExpr),
    Unary(UnaryExpr),
    Cond(CondExpr),
    Ident(IdentExpr),
    Call(CallExpr),
    If(IfExpr),
    MemberAccess(MemberAccessExpr),
    Paren(ParenExpr),
    Index(IndexExpr),
    Match(MatchExpr),
    Range(RangeExpr),
    TmpVar(TmpVarExpr),
    BlockExpr(BlockExpr),
    Import(ImportExpr),
    Literal(Literal),
}

// Define the BinaryExpr struct
#[derive(Debug)]
pub struct BinaryExpr {
    pub op: BinaryOperator,
    pub left: Option<Rc<Expr>>,
    pub right: Option<Rc<Expr>>,
}

#[derive(Debug)]
pub enum BinaryOperator {
    Add,               // +
    Subtract,          // -
    Multiply,          // *
    Divide,            // /
    Modulus,           // %
    BitwiseAnd,        // &
    BitwiseOr,         // |
    BitwiseXor,        // ^
    ShiftLeft,         // <<
    ShiftRight,        // >>
    Equal,             // ==
    NotEqual,          // !=
    Less,              // <
    LessEqual,         // <=
    Greater,           // >
    GreaterEqual,      // >=
    LogicalAnd,        // &&
    LogicalOr,         // ||
    Assignment,        // =
    ColonAssign,       // :=
    DoubleColonAssign, // ::=
    PlusAssign,        // +=
    MinusAssign,       // -=
    MultiplyAssign,    // *=
    DivideAssign,      // /=
    ModulusAssign,     // %=
    ShiftLeftAssign,   // <<=
    ShiftRightAssign,  // >>=
    BitwiseAndAssign,  // &=
    BitwiseOrAssign,   // |=
    BitwiseXorAssign,  // ^=
    Range,             // ..
    InclusiveRange,    // ..=
    Comma,             // ,
}

// Define the UnaryExpr struct
#[derive(Debug)]
pub struct UnaryExpr {
    pub op: UnaryOperator,
    pub expr: Option<Rc<Expr>>,
}

#[derive(Debug)]
pub enum UnaryOperator {
    Not,
    Negate,
}

// Define the CondExpr struct
#[derive(Debug)]
pub struct CondExpr {
    pub cond: Option<Rc<Expr>>,
    pub then: Option<Rc<Expr>>,
    pub els: Option<Rc<Expr>>,
}

// Define the IdentExpr struct
#[derive(Debug)]
pub struct IdentExpr {
    pub ident: String,
    pub usage: IdentUsage,
    pub base: Option<Weak<Node>>,
    pub scope: Option<Rc<Scope>>,
}

#[derive(Debug)]
pub enum IdentUsage {
    Unknown,
    Reference,
    DefineVariable,
    DefineConst,
    DefineField,
    DefineFormat,
    DefineFn,
    ReferenceType,
}

// Define the CallExpr struct
#[derive(Debug)]
pub struct CallExpr {
    pub callee: Option<Rc<Expr>>,
    pub raw_arguments: Option<Rc<Expr>>,
    pub arguments: Vec<Option<Rc<Expr>>>,
    pub end_loc: Option<Loc>,
}

// Define the IfExpr struct
#[derive(Debug)]
pub struct IfExpr {
    pub cond: Option<Rc<Expr>>,
    pub then: Option<Rc<IndentScope>>,
    pub els: Option<Rc<Node>>,
}

// Define the MemberAccessExpr struct
#[derive(Debug)]
pub struct MemberAccessExpr {
    pub target: Option<Rc<Expr>>,
    pub member: String,
    pub member_loc: Option<Loc>,
}

// Define the ParenExpr struct
#[derive(Debug)]
pub struct ParenExpr {
    pub expr: Option<Rc<Expr>>,
    pub end_loc: Option<Loc>,
}

// Define the IndexExpr struct
#[derive(Debug)]
pub struct IndexExpr {
    pub expr: Option<Rc<Expr>>,
    pub index: Option<Rc<Expr>>,
    pub end_loc: Option<Loc>,
}

// Define the MatchExpr struct
#[derive(Debug)]
pub struct MatchExpr {
    pub cond: Option<Rc<Expr>>,
    pub branch: Vec<Option<Rc<MatchBranch>>>,
    pub scope: Option<Rc<Scope>>,
}

// Define the MatchBranch struct
#[derive(Debug)]
pub struct MatchBranch {
    pub cond: Option<Rc<Expr>>,
    pub sym_loc: Option<Loc>,
    pub then: Option<Rc<Node>>,
}

// Define the RangeExpr struct
#[derive(Debug)]
pub struct RangeExpr {
    pub op: BinaryOperator, // or another appropriate type
    pub start: Option<Rc<Expr>>,
    pub end: Option<Rc<Expr>>,
}

// Define the TmpVarExpr struct
#[derive(Debug)]
pub struct TmpVarExpr {
    pub tmp_var: u32,
}

// Define the BlockExpr struct
#[derive(Debug)]
pub struct BlockExpr {
    pub calls: Vec<Option<Rc<Node>>>,
    pub expr: Option<Rc<Expr>>,
}

// Define the ImportExpr struct
#[derive(Debug)]
pub struct ImportExpr {
    pub path: String,
    pub base: Option<Rc<CallExpr>>,
    pub import_desc: Option<Rc<Program>>,
}

// Define the Literal enum
#[derive(Debug)]
pub enum Literal {
    IntLiteral(IntLiteral),
    BoolLiteral(BoolLiteral),
    StrLiteral(StrLiteral),
    Input,
    Output,
    Config,
}

// Define the IntLiteral struct
#[derive(Debug)]
pub struct IntLiteral {
    pub value: String,
}

// Define the BoolLiteral struct
#[derive(Debug)]
pub struct BoolLiteral {
    pub value: bool,
}

// Define the StrLiteral struct
#[derive(Debug)]
pub struct StrLiteral {
    pub value: String,
}

// Define the Stmt enum
#[derive(Debug)]
pub enum Stmt {
    Loop(LoopStmt),
    IndentScope(IndentScope),
    MatchBranch(MatchBranchStmt),
    Return(ReturnStmt),
    Break,
    Continue,
    Assert(AssertStmt),
    ImplicitYield(ImplicitYieldStmt),
    Member(MemberStmt),
}

#[derive(Debug)]
pub struct AssertStmt {
    pub cond: Option<Rc<BinaryExpr>>,
}

#[derive(Debug)]
pub struct ImplicitYieldStmt {
    pub expr: Option<Rc<Expr>>,
}

// Define the LoopStmt struct
#[derive(Debug)]
pub struct LoopStmt {
    pub init: Option<Rc<Expr>>,
    pub cond: Option<Rc<Expr>>,
    pub step: Option<Rc<Expr>>,
    pub body: Option<Rc<IndentScope>>,
}

// Define the IndentScopeStmt struct
#[derive(Debug)]
pub struct IndentScope {
    pub elements: Vec<Option<Rc<Node>>>,
    pub scope: Option<Rc<Scope>>,
}

// Define the MatchBranchStmt struct
#[derive(Debug)]
pub struct MatchBranchStmt {
    pub cond: Option<Rc<Expr>>,
    pub sym_loc: Option<Loc>,
    pub then: Option<Rc<Node>>,
}

// Define the ReturnStmt struct
#[derive(Debug)]
pub struct ReturnStmt {
    pub expr: Option<Rc<Expr>>,
}

// Define the MemberStmt enum
#[derive(Debug)]
pub enum MemberStmt {
    Field(FieldStmt),
    Format(FormatStmt),
    Function(FunctionStmt),
}

// Define the FieldStmt struct
#[derive(Debug)]
pub struct FieldStmt {
    pub ident: Option<Rc<IdentExpr>>,
    pub colon_loc: Option<Loc>,
    pub field_type: Option<Rc<Type>>,
    pub raw_arguments: Option<Rc<Expr>>,
    pub arguments: Vec<Option<Rc<Expr>>>,
    pub belong: Option<Weak<FormatStmt>>,
}

// Define the FormatStmt struct
#[derive(Debug)]
pub struct FormatStmt {
    pub is_enum: bool,
    pub ident: Option<Rc<IdentExpr>>,
    pub body: Option<Rc<IndentScope>>,
    pub belong: Option<Weak<FormatStmt>>,
    pub struct_type: Option<Rc<StructType>>,
}

// Define the FunctionStmt struct
#[derive(Debug)]
pub struct FunctionStmt {
    pub ident: Option<Rc<IdentExpr>>,
    pub parameters: Vec<Option<Rc<FieldStmt>>>,
    pub return_type: Option<Rc<Type>>,
    pub belong: Option<Weak<FormatStmt>>,
    pub body: Option<Rc<IndentScope>>,
    pub func_type: Option<Rc<FunctionType>>,
}

// Define the Type enum
#[derive(Debug)]
pub enum Type {
    IntType(IntType),
    IdentType(IdentType),
    IntLiteralType(IntLiteralType),
    StrLiteralType(StrLiteralType),
    VoidType,
    BoolType,
    ArrayType(ArrayType),
    FunctionType(FunctionType),
    StructType(StructType),
    UnionType(UnionType),
}

// Define the IntType struct
#[derive(Debug)]
pub struct IntType {
    pub raw: String,
    pub bit_size: u32,
}

// Define the IdentType struct
#[derive(Debug)]
pub struct IdentType {
    pub ident: Option<Rc<IdentExpr>>,
    pub base: Option<Weak<FormatStmt>>,
}

// Define the IntLiteralType struct
#[derive(Debug)]
pub struct IntLiteralType {
    pub base: Option<Weak<IntLiteral>>,
}

// Define the StrLiteralType struct
#[derive(Debug)]
pub struct StrLiteralType {
    pub base: Option<Weak<StrLiteral>>,
}

// Define the ArrayType struct
#[derive(Debug)]
pub struct ArrayType {
    pub base_type: Option<Rc<Type>>,
    pub length: Option<Rc<Expr>>,
}

// Define the FunctionType struct
#[derive(Debug)]
pub struct FunctionType {
    pub return_type: Option<Rc<Type>>,
    pub parameters: Vec<Option<Rc<Type>>>,
}

// Define the StructType struct
#[derive(Debug)]
pub struct StructType {
    pub fields: Vec<Option<Rc<MemberStmt>>>,
}

// Define the UnionType struct
#[derive(Debug)]
pub struct UnionType {
    pub fields: Vec<Option<Rc<StructType>>>,
}

// Define the Scope struct
#[derive(Debug)]
pub struct Scope {
    pub prev: Option<Weak<Scope>>,
    pub next: Option<Rc<Scope>>,
    pub branch: Option<Rc<Scope>>,
    pub ident: Vec<Option<Weak<Node>>>,
}
