library ast;
enum NodeType {
Program,
Comment,
CommentGroup,
FieldArgument,
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
Import,
Cast,
Available,
SpecifyOrder,
ExplicitError,
IoOperation,
BadExpr,
Stmt,
Loop,
IndentBlock,
ScopedStatement,
MatchBranch,
UnionCandidate,
Return,
Break,
Continue,
Assert,
ImplicitYield,
Metadata,
Type,
IntType,
FloatType,
IdentType,
IntLiteralType,
StrLiteralType,
VoidType,
BoolType,
ArrayType,
FunctionType,
StructType,
StructUnionType,
UnionType,
RangeType,
EnumType,
MetaType,
OptionalType,
GenericType,
Literal,
IntLiteral,
BoolLiteral,
StrLiteral,
CharLiteral,
TypeLiteral,
SpecialLiteral,
Member,
Field,
Format,
State,
Enum,
EnumMember,
Function,
BuiltinMember,
BuiltinFunction,
BuiltinField,
BuiltinObject,
}
enum TokenTag {
Indent,
Space,
Line,
Punct,
IntLiteral,
BoolLiteral,
StrLiteral,
CharLiteral,
Keyword,
Ident,
Comment,
Error,
Unknown,
}
enum UnaryOp {
Not,
MinusSign,
}
enum BinaryOp {
Mul,
Div,
Mod,
LeftArithmeticShift,
RightArithmeticShift,
LeftLogicalShift,
RightLogicalShift,
BitAnd,
Add,
Sub,
BitOr,
BitXor,
Equal,
NotEqual,
Less,
LessOrEq,
Grater,
GraterOrEq,
LogicalAnd,
LogicalOr,
CondOp1,
CondOp2,
RangeExclusive,
RangeInclusive,
Assign,
DefineAssign,
ConstAssign,
AddAssign,
SubAssign,
MulAssign,
DivAssign,
ModAssign,
LeftShiftAssign,
RightShiftAssign,
BitAndAssign,
BitOrAssign,
BitXorAssign,
Comma,
}
enum IdentUsage {
Unknown,
Reference,
DefineVariable,
DefineConst,
DefineField,
DefineFormat,
DefineState,
DefineEnum,
DefineEnumMember,
DefineFn,
DefineCastFn,
DefineArg,
ReferenceType,
ReferenceMember,
ReferenceMemberType,
MaybeType,
ReferenceBuiltinFn,
}
enum Endian {
Unspec,
Big,
Little,
}
enum ConstantLevel {
Unknown,
Constant,
ImmutableVariable,
Variable,
}
enum BitAlignment {
ByteAligned,
Bit1,
Bit2,
Bit3,
Bit4,
Bit5,
Bit6,
Bit7,
NotTarget,
NotDecidable,
}
enum Follow {
Unknown,
End,
Fixed,
Constant,
Normal,
}
enum IoMethod {
Unspec,
OutputPut,
InputPeek,
InputGet,
InputBackward,
InputOffset,
InputBitOffset,
InputRemain,
InputSubrange,
ConfigEndianLittle,
ConfigEndianBig,
ConfigEndianNative,
ConfigBitOrderLsb,
ConfigBitOrderMsb,
}
enum SpecialLiteralKind {
Input,
Output,
Config,
}
enum OrderType {
Byte,
BitStream,
BitMapping,
BitBoth,
}
abstract class Node {
Loc loc = Loc();
}
abstract class Expr extends Node {
Type? exprType;
ConstantLevel constantLevel = ConstantLevel.Unknown;
}
abstract class Stmt extends Node {
}
abstract class Type extends Node {
bool isExplicit = false;
bool nonDynamicAllocation = false;
BitAlignment bitAlignment = BitAlignment.ByteAligned;
int? bitSize;
}
abstract class Literal extends Expr {
}
abstract class Member extends Stmt {
Member? belong;
StructType? belongStruct;
Ident? ident;
}
abstract class BuiltinMember extends Member {
}
class Program extends Node {
StructType? structType;
List<Node>? elements = [];
Scope? globalScope;
}
class Comment extends Node {
String comment = '';
}
class CommentGroup extends Node {
List<Comment>? comments = [];
}
class FieldArgument extends Node {
Expr? rawArguments;
Loc endLoc = Loc();
List<Expr>? collectedArguments = [];
List<Expr>? arguments = [];
Expr? alignment;
int? alignmentValue;
Expr? subByteLength;
Expr? subByteBegin;
Expr? peek;
int? peekValue;
TypeLiteral? typeMap;
List<Metadata>? metadata = [];
}
class Binary extends Expr {
BinaryOp op = BinaryOp.Mul;
Expr? left;
Expr? right;
}
class Unary extends Expr {
UnaryOp op = UnaryOp.Not;
Expr? expr;
}
class Cond extends Expr {
Expr? cond;
Expr? then;
Loc elsLoc = Loc();
Expr? els;
}
class Ident extends Expr {
String ident = '';
IdentUsage usage = IdentUsage.Unknown;
Node? base;
Scope? scope;
}
class Call extends Expr {
Expr? callee;
Expr? rawArguments;
List<Expr>? arguments = [];
Loc endLoc = Loc();
}
class If extends Expr {
StructUnionType? structUnionType;
Scope? condScope;
Expr? cond;
IndentBlock? then;
Node? els;
}
class MemberAccess extends Expr {
Expr? target;
Ident? member;
Ident? base;
}
class Paren extends Expr {
Expr? expr;
Loc endLoc = Loc();
}
class Index extends Expr {
Expr? expr;
Expr? index;
Loc endLoc = Loc();
}
class Match extends Expr {
StructUnionType? structUnionType;
Scope? condScope;
Expr? cond;
List<MatchBranch>? branch = [];
}
class Range extends Expr {
BinaryOp op = BinaryOp.Mul;
Expr? start;
Expr? end;
}
class TmpVar extends Expr {
int tmpVar = 0;
}
class Import extends Expr {
String path = '';
Call? base;
Program? importDesc;
}
class Cast extends Expr {
Call? base;
Expr? expr;
}
class Available extends Expr {
Call? base;
Expr? target;
}
class SpecifyOrder extends Expr {
Binary? base;
OrderType orderType = OrderType.Byte;
Expr? order;
int? orderValue;
}
class ExplicitError extends Expr {
Call? base;
StrLiteral? message;
}
class IoOperation extends Expr {
Expr? base;
IoMethod method = IoMethod.Unspec;
List<Expr>? arguments = [];
}
class BadExpr extends Expr {
String content = '';
}
class Loop extends Stmt {
Scope? condScope;
Expr? init;
Expr? cond;
Expr? step;
IndentBlock? body;
}
class IndentBlock extends Stmt {
StructType? structType;
List<Node>? elements = [];
Scope? scope;
}
class ScopedStatement extends Stmt {
StructType? structType;
Node? statement;
Scope? scope;
}
class MatchBranch extends Stmt {
Match? belong;
Expr? cond;
Loc symLoc = Loc();
Node? then;
}
class UnionCandidate extends Stmt {
Expr? cond;
Field? field;
}
class Return extends Stmt {
Expr? expr;
}
class Break extends Stmt {
}
class Continue extends Stmt {
}
class Assert extends Stmt {
Binary? cond;
bool isIoRelated = false;
}
class ImplicitYield extends Stmt {
Expr? expr;
}
class Metadata extends Stmt {
Expr? base;
String name = '';
List<Expr>? values = [];
}
class IntType extends Type {
Endian endian = Endian.Unspec;
bool isSigned = false;
bool isCommonSupported = false;
}
class FloatType extends Type {
Endian endian = Endian.Unspec;
bool isCommonSupported = false;
}
class IdentType extends Type {
MemberAccess? importRef;
Ident? ident;
Type? base;
}
class IntLiteralType extends Type {
IntLiteral? base;
}
class StrLiteralType extends Type {
StrLiteral? base;
StrLiteral? strongRef;
}
class VoidType extends Type {
}
class BoolType extends Type {
}
class ArrayType extends Type {
Loc endLoc = Loc();
Type? elementType;
Expr? length;
int? lengthValue;
}
class FunctionType extends Type {
Type? returnType;
List<Type>? parameters = [];
}
class StructType extends Type {
List<Member>? fields = [];
Node? base;
bool recursive = false;
int fixedHeaderSize = 0;
int fixedTailSize = 0;
}
class StructUnionType extends Type {
Expr? cond;
List<Expr>? conds = [];
List<StructType>? structs = [];
Expr? base;
List<Field>? unionFields = [];
bool exhaustive = false;
}
class UnionType extends Type {
Expr? cond;
List<UnionCandidate>? candidates = [];
StructUnionType? baseType;
Type? commonType;
}
class RangeType extends Type {
Type? baseType;
Range? range;
}
class EnumType extends Type {
Enum? base;
}
class MetaType extends Type {
}
class OptionalType extends Type {
Type? baseType;
}
class GenericType extends Type {
Member? belong;
}
class IntLiteral extends Literal {
String value = '';
}
class BoolLiteral extends Literal {
bool value = false;
}
class StrLiteral extends Literal {
String value = '';
int length = 0;
}
class CharLiteral extends Literal {
String value = '';
int code = 0;
}
class TypeLiteral extends Literal {
Type? typeLiteral;
Loc endLoc = Loc();
}
class SpecialLiteral extends Literal {
SpecialLiteralKind kind = SpecialLiteralKind.Input;
}
class Field extends Member {
Loc colonLoc = Loc();
bool isStateVariable = false;
Type? fieldType;
FieldArgument? arguments;
int? offsetBit;
int offsetRecent = 0;
int? tailOffsetBit;
int tailOffsetRecent = 0;
BitAlignment bitAlignment = BitAlignment.ByteAligned;
BitAlignment eventualBitAlignment = BitAlignment.ByteAligned;
Follow follow = Follow.Unknown;
Follow eventualFollow = Follow.Unknown;
Field? next;
}
class Format extends Member {
IndentBlock? body;
Func? encodeFn;
Func? decodeFn;
List<Func>? castFns = [];
List<IdentType>? depends = [];
List<Field>? stateVariables = [];
}
class State extends Member {
IndentBlock? body;
}
class Enum extends Member {
Scope? scope;
Loc colonLoc = Loc();
Type? baseType;
List<EnumMember>? members = [];
EnumType? enumType;
}
class EnumMember extends Member {
Expr? rawExpr;
Expr? value;
StrLiteral? strLiteral;
}
class Func extends Member {
List<Field>? parameters = [];
Type? returnType;
IndentBlock? body;
FunctionType? funcType;
bool isCast = false;
Loc castLoc = Loc();
}
class BuiltinFunction extends Member {
FunctionType? funcType;
}
class BuiltinField extends Member {
Type? fieldType;
}
class BuiltinObject extends Member {
List<BuiltinMember>? members = [];
}
class Scope {
Scope? prev;
Scope? next;
Scope? branch;
List<Ident>? ident = [];
Node? owner;
bool branchRoot = false;
}
class Pos {
int begin = 0;
int end = 0;
}
class Loc {
Pos pos = Pos();
int file = 0;
int line = 0;
int col = 0;
}
class Token {
TokenTag tag = TokenTag.Indent;
String token = '';
Loc loc = Loc();
}
class RawScope {
int? prev;
int? next;
int? branch;
List<int> ident = [];
int? owner;
bool branchRoot = false;
}
class RawNode {
NodeType nodeType = NodeType.Program;
Loc loc = Loc();
dynamic body;
}
class SrcErrorEntry {
String msg = '';
String file = '';
Loc loc = Loc();
String src = '';
bool warn = false;
}
class SrcError {
List<SrcErrorEntry> errs = [];
}
class JsonAst {
List<RawNode> node = [];
List<RawScope> scope = [];
}
class AstFile {
List<String> files = [];
JsonAst? ast;
SrcError? error;
}
class TokenFile {
List<String> files = [];
List<Token>? tokens = [];
SrcError? error;
}
