use std::rc::{Rc, Weak};

use serde_derive::{Deserialize, Serialize};

// Define the enum for the "one_of" field
#[derive(Debug)]
pub enum Node {
    Program(Program),
    Expr(Expr),
    Stmt(Stmt),
    Type(Type),
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
    pub then_expr: Option<Rc<Expr>>,
    pub else_expr: Option<Rc<Expr>>,
}

// Define the IdentExpr struct
#[derive(Debug)]
pub struct IdentExpr {
    pub ident: String,
    pub usage: Vec<IdentUsage>,
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
    Assert(BinaryExpr),
    ImplicitYield(Expr),
    Member(MemberStmt),
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
