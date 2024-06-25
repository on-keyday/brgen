library ast;
import 'package:json_annotation/json_annotation.dart';
enum NodeType {
@JsonValue('program')
Program,
@JsonValue('comment')
Comment,
@JsonValue('comment_group')
CommentGroup,
@JsonValue('field_argument')
FieldArgument,
@JsonValue('expr')
Expr,
@JsonValue('binary')
Binary,
@JsonValue('unary')
Unary,
@JsonValue('cond')
Cond,
@JsonValue('ident')
Ident,
@JsonValue('call')
Call,
@JsonValue('if')
If,
@JsonValue('member_access')
MemberAccess,
@JsonValue('paren')
Paren,
@JsonValue('index')
Index,
@JsonValue('match')
Match,
@JsonValue('range')
Range,
@JsonValue('identity')
Identity,
@JsonValue('tmp_var')
TmpVar,
@JsonValue('import')
Import,
@JsonValue('cast')
Cast,
@JsonValue('available')
Available,
@JsonValue('specify_order')
SpecifyOrder,
@JsonValue('explicit_error')
ExplicitError,
@JsonValue('io_operation')
IoOperation,
@JsonValue('or_cond')
OrCond,
@JsonValue('bad_expr')
BadExpr,
@JsonValue('stmt')
Stmt,
@JsonValue('loop')
Loop,
@JsonValue('indent_block')
IndentBlock,
@JsonValue('scoped_statement')
ScopedStatement,
@JsonValue('match_branch')
MatchBranch,
@JsonValue('union_candidate')
UnionCandidate,
@JsonValue('return')
Return,
@JsonValue('break')
Break,
@JsonValue('continue')
Continue,
@JsonValue('assert')
Assert,
@JsonValue('implicit_yield')
ImplicitYield,
@JsonValue('metadata')
Metadata,
@JsonValue('type')
Type,
@JsonValue('int_type')
IntType,
@JsonValue('float_type')
FloatType,
@JsonValue('ident_type')
IdentType,
@JsonValue('int_literal_type')
IntLiteralType,
@JsonValue('str_literal_type')
StrLiteralType,
@JsonValue('regex_literal_type')
RegexLiteralType,
@JsonValue('void_type')
VoidType,
@JsonValue('bool_type')
BoolType,
@JsonValue('array_type')
ArrayType,
@JsonValue('function_type')
FunctionType,
@JsonValue('struct_type')
StructType,
@JsonValue('struct_union_type')
StructUnionType,
@JsonValue('union_type')
UnionType,
@JsonValue('range_type')
RangeType,
@JsonValue('enum_type')
EnumType,
@JsonValue('meta_type')
MetaType,
@JsonValue('optional_type')
OptionalType,
@JsonValue('generic_type')
GenericType,
@JsonValue('literal')
Literal,
@JsonValue('int_literal')
IntLiteral,
@JsonValue('bool_literal')
BoolLiteral,
@JsonValue('str_literal')
StrLiteral,
@JsonValue('regex_literal')
RegexLiteral,
@JsonValue('char_literal')
CharLiteral,
@JsonValue('type_literal')
TypeLiteral,
@JsonValue('special_literal')
SpecialLiteral,
@JsonValue('member')
Member,
@JsonValue('field')
Field,
@JsonValue('format')
Format,
@JsonValue('state')
State,
@JsonValue('enum')
Enum,
@JsonValue('enum_member')
EnumMember,
@JsonValue('function')
Function,
}
enum TokenTag {
@JsonValue('indent')
Indent,
@JsonValue('space')
Space,
@JsonValue('line')
Line,
@JsonValue('punct')
Punct,
@JsonValue('int_literal')
IntLiteral,
@JsonValue('bool_literal')
BoolLiteral,
@JsonValue('str_literal')
StrLiteral,
@JsonValue('regex_literal')
RegexLiteral,
@JsonValue('char_literal')
CharLiteral,
@JsonValue('keyword')
Keyword,
@JsonValue('ident')
Ident,
@JsonValue('comment')
Comment,
@JsonValue('error')
Error,
@JsonValue('unknown')
Unknown,
}
enum UnaryOp {
@JsonValue('!')
Not,
@JsonValue('-')
MinusSign,
}
enum BinaryOp {
@JsonValue('*')
Mul,
@JsonValue('/')
Div,
@JsonValue('%')
Mod,
@JsonValue('<<<')
LeftArithmeticShift,
@JsonValue('>>>')
RightArithmeticShift,
@JsonValue('<<')
LeftLogicalShift,
@JsonValue('>>')
RightLogicalShift,
@JsonValue('&')
BitAnd,
@JsonValue('+')
Add,
@JsonValue('-')
Sub,
@JsonValue('|')
BitOr,
@JsonValue('^')
BitXor,
@JsonValue('==')
Equal,
@JsonValue('!=')
NotEqual,
@JsonValue('<')
Less,
@JsonValue('<=')
LessOrEq,
@JsonValue('>')
Grater,
@JsonValue('>=')
GraterOrEq,
@JsonValue('&&')
LogicalAnd,
@JsonValue('||')
LogicalOr,
@JsonValue('?')
CondOp1,
@JsonValue(':')
CondOp2,
@JsonValue('..')
RangeExclusive,
@JsonValue('..=')
RangeInclusive,
@JsonValue('=')
Assign,
@JsonValue(':=')
DefineAssign,
@JsonValue('::=')
ConstAssign,
@JsonValue('+=')
AddAssign,
@JsonValue('-=')
SubAssign,
@JsonValue('*=')
MulAssign,
@JsonValue('/=')
DivAssign,
@JsonValue('%=')
ModAssign,
@JsonValue('<<=')
LeftLogicalShiftAssign,
@JsonValue('>>=')
RightLogicalShiftAssign,
@JsonValue('<<<=')
LeftArithmeticShiftAssign,
@JsonValue('>>>=')
RightArithmeticShiftAssign,
@JsonValue('&=')
BitAndAssign,
@JsonValue('|=')
BitOrAssign,
@JsonValue('^=')
BitXorAssign,
@JsonValue(',')
Comma,
@JsonValue('in')
InAssign,
}
enum IdentUsage {
@JsonValue('unknown')
Unknown,
@JsonValue('bad_ident')
BadIdent,
@JsonValue('reference')
Reference,
@JsonValue('define_variable')
DefineVariable,
@JsonValue('define_const')
DefineConst,
@JsonValue('define_field')
DefineField,
@JsonValue('define_format')
DefineFormat,
@JsonValue('define_state')
DefineState,
@JsonValue('define_enum')
DefineEnum,
@JsonValue('define_enum_member')
DefineEnumMember,
@JsonValue('define_fn')
DefineFn,
@JsonValue('define_cast_fn')
DefineCastFn,
@JsonValue('define_arg')
DefineArg,
@JsonValue('reference_type')
ReferenceType,
@JsonValue('reference_member')
ReferenceMember,
@JsonValue('reference_member_type')
ReferenceMemberType,
@JsonValue('maybe_type')
MaybeType,
@JsonValue('reference_builtin_fn')
ReferenceBuiltinFn,
}
enum Endian {
@JsonValue('unspec')
Unspec,
@JsonValue('big')
Big,
@JsonValue('little')
Little,
}
enum ConstantLevel {
@JsonValue('unknown')
Unknown,
@JsonValue('constant')
Constant,
@JsonValue('immutable_variable')
ImmutableVariable,
@JsonValue('variable')
Variable,
}
enum BitAlignment {
@JsonValue('byte_aligned')
ByteAligned,
@JsonValue('bit_1')
Bit1,
@JsonValue('bit_2')
Bit2,
@JsonValue('bit_3')
Bit3,
@JsonValue('bit_4')
Bit4,
@JsonValue('bit_5')
Bit5,
@JsonValue('bit_6')
Bit6,
@JsonValue('bit_7')
Bit7,
@JsonValue('not_target')
NotTarget,
@JsonValue('not_decidable')
NotDecidable,
}
enum Follow {
@JsonValue('unknown')
Unknown,
@JsonValue('end')
End,
@JsonValue('fixed')
Fixed,
@JsonValue('constant')
Constant,
@JsonValue('normal')
Normal,
}
enum IoMethod {
@JsonValue('unspec')
Unspec,
@JsonValue('output_put')
OutputPut,
@JsonValue('input_peek')
InputPeek,
@JsonValue('input_get')
InputGet,
@JsonValue('input_backward')
InputBackward,
@JsonValue('input_offset')
InputOffset,
@JsonValue('input_bit_offset')
InputBitOffset,
@JsonValue('input_remain')
InputRemain,
@JsonValue('input_subrange')
InputSubrange,
@JsonValue('config_endian_little')
ConfigEndianLittle,
@JsonValue('config_endian_big')
ConfigEndianBig,
@JsonValue('config_endian_native')
ConfigEndianNative,
@JsonValue('config_bit_order_lsb')
ConfigBitOrderLsb,
@JsonValue('config_bit_order_msb')
ConfigBitOrderMsb,
}
enum SpecialLiteralKind {
@JsonValue('input')
Input,
@JsonValue('output')
Output,
@JsonValue('config')
Config,
}
enum OrderType {
@JsonValue('byte')
Byte,
@JsonValue('bit_stream')
BitStream,
@JsonValue('bit_mapping')
BitMapping,
@JsonValue('bit_both')
BitBoth,
}
enum BlockTrait {
@JsonValue('none')
None,
@JsonValue('fixed_primitive')
FixedPrimitive,
@JsonValue('fixed_float')
FixedFloat,
@JsonValue('fixed_array')
FixedArray,
@JsonValue('variable_array')
VariableArray,
@JsonValue('struct')
Struct,
@JsonValue('conditional')
Conditional,
@JsonValue('static_peek')
StaticPeek,
@JsonValue('bit_field')
BitField,
@JsonValue('read_state')
ReadState,
@JsonValue('write_state')
WriteState,
@JsonValue('terminal_pattern')
TerminalPattern,
@JsonValue('bit_stream')
BitStream,
@JsonValue('dynamic_order')
DynamicOrder,
@JsonValue('full_input')
FullInput,
@JsonValue('backward_input')
BackwardInput,
@JsonValue('magic_value')
MagicValue,
@JsonValue('assertion')
Assertion,
@JsonValue('explicit_error')
ExplicitError,
@JsonValue('procedural')
Procedural,
@JsonValue('for_loop')
ForLoop,
@JsonValue('local_variable')
LocalVariable,
@JsonValue('description_only')
DescriptionOnly,
@JsonValue('uncommon_size')
UncommonSize,
@JsonValue('control_flow_change')
ControlFlowChange,
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
@JsonSerializable()
class Program extends Node {
    StructType? structType;
    List<Node>? elements = [];
    Scope? globalScope;
    List<Metadata>? metadata = [];
factory Program.fromJson(Map<String, dynamic> json) => _$ProgramFromJson(json);
}
@JsonSerializable()
class Comment extends Node {
    String comment = '';
factory Comment.fromJson(Map<String, dynamic> json) => _$CommentFromJson(json);
}
@JsonSerializable()
class CommentGroup extends Node {
    List<Comment>? comments = [];
factory CommentGroup.fromJson(Map<String, dynamic> json) => _$CommentGroupFromJson(json);
}
@JsonSerializable()
class FieldArgument extends Node {
    Expr? rawArguments;
    Loc endLoc = Loc();
    List<Expr>? collectedArguments = [];
    List<Expr>? arguments = [];
    List<Binary>? assigns = [];
    Expr? alignment;
    int? alignmentValue;
    Expr? subByteLength;
    Expr? subByteBegin;
    Expr? peek;
    int? peekValue;
    TypeLiteral? typeMap;
    List<Metadata>? metadata = [];
factory FieldArgument.fromJson(Map<String, dynamic> json) => _$FieldArgumentFromJson(json);
}
@JsonSerializable()
class Binary extends Expr {
    BinaryOp op = BinaryOp.Mul;
    Expr? left;
    Expr? right;
factory Binary.fromJson(Map<String, dynamic> json) => _$BinaryFromJson(json);
}
@JsonSerializable()
class Unary extends Expr {
    UnaryOp op = UnaryOp.Not;
    Expr? expr;
factory Unary.fromJson(Map<String, dynamic> json) => _$UnaryFromJson(json);
}
@JsonSerializable()
class Cond extends Expr {
    Expr? cond;
    Expr? then;
    Loc elsLoc = Loc();
    Expr? els;
factory Cond.fromJson(Map<String, dynamic> json) => _$CondFromJson(json);
}
@JsonSerializable()
class Ident extends Expr {
    String ident = '';
    IdentUsage usage = IdentUsage.Unknown;
    Node? base;
    Scope? scope;
factory Ident.fromJson(Map<String, dynamic> json) => _$IdentFromJson(json);
}
@JsonSerializable()
class Call extends Expr {
    Expr? callee;
    Expr? rawArguments;
    List<Expr>? arguments = [];
    Loc endLoc = Loc();
factory Call.fromJson(Map<String, dynamic> json) => _$CallFromJson(json);
}
@JsonSerializable()
class If extends Expr {
    StructUnionType? structUnionType;
    Scope? condScope;
    Identity? cond;
    IndentBlock? then;
    Node? els;
factory If.fromJson(Map<String, dynamic> json) => _$IfFromJson(json);
}
@JsonSerializable()
class MemberAccess extends Expr {
    Expr? target;
    Ident? member;
    Ident? base;
factory MemberAccess.fromJson(Map<String, dynamic> json) => _$MemberAccessFromJson(json);
}
@JsonSerializable()
class Paren extends Expr {
    Expr? expr;
    Loc endLoc = Loc();
factory Paren.fromJson(Map<String, dynamic> json) => _$ParenFromJson(json);
}
@JsonSerializable()
class Index extends Expr {
    Expr? expr;
    Expr? index;
    Loc endLoc = Loc();
factory Index.fromJson(Map<String, dynamic> json) => _$IndexFromJson(json);
}
@JsonSerializable()
class Match extends Expr {
    StructUnionType? structUnionType;
    Scope? condScope;
    Identity? cond;
    List<MatchBranch>? branch = [];
factory Match.fromJson(Map<String, dynamic> json) => _$MatchFromJson(json);
}
@JsonSerializable()
class Range extends Expr {
    BinaryOp op = BinaryOp.Mul;
    Expr? start;
    Expr? end;
factory Range.fromJson(Map<String, dynamic> json) => _$RangeFromJson(json);
}
@JsonSerializable()
class Identity extends Expr {
    Expr? expr;
factory Identity.fromJson(Map<String, dynamic> json) => _$IdentityFromJson(json);
}
@JsonSerializable()
class TmpVar extends Expr {
    int tmpVar = 0;
factory TmpVar.fromJson(Map<String, dynamic> json) => _$TmpVarFromJson(json);
}
@JsonSerializable()
class Import extends Expr {
    String path = '';
    Call? base;
    Program? importDesc;
factory Import.fromJson(Map<String, dynamic> json) => _$ImportFromJson(json);
}
@JsonSerializable()
class Cast extends Expr {
    Call? base;
    List<Expr>? arguments = [];
factory Cast.fromJson(Map<String, dynamic> json) => _$CastFromJson(json);
}
@JsonSerializable()
class Available extends Expr {
    Call? base;
    Expr? target;
factory Available.fromJson(Map<String, dynamic> json) => _$AvailableFromJson(json);
}
@JsonSerializable()
class SpecifyOrder extends Expr {
    Binary? base;
    OrderType orderType = OrderType.Byte;
    Expr? order;
    int? orderValue;
factory SpecifyOrder.fromJson(Map<String, dynamic> json) => _$SpecifyOrderFromJson(json);
}
@JsonSerializable()
class ExplicitError extends Expr {
    Call? base;
    StrLiteral? message;
factory ExplicitError.fromJson(Map<String, dynamic> json) => _$ExplicitErrorFromJson(json);
}
@JsonSerializable()
class IoOperation extends Expr {
    Expr? base;
    IoMethod method = IoMethod.Unspec;
    List<Expr>? arguments = [];
factory IoOperation.fromJson(Map<String, dynamic> json) => _$IoOperationFromJson(json);
}
@JsonSerializable()
class OrCond extends Expr {
    Binary? base;
    List<Expr>? conds = [];
factory OrCond.fromJson(Map<String, dynamic> json) => _$OrCondFromJson(json);
}
@JsonSerializable()
class BadExpr extends Expr {
    String content = '';
    Expr? badExpr;
factory BadExpr.fromJson(Map<String, dynamic> json) => _$BadExprFromJson(json);
}
@JsonSerializable()
class Loop extends Stmt {
    Scope? condScope;
    Expr? init;
    Expr? cond;
    Expr? step;
    IndentBlock? body;
factory Loop.fromJson(Map<String, dynamic> json) => _$LoopFromJson(json);
}
@JsonSerializable()
class IndentBlock extends Stmt {
    StructType? structType;
    List<Node>? elements = [];
    Scope? scope;
    List<Metadata>? metadata = [];
    BlockTrait blockTraits = BlockTrait.None;
factory IndentBlock.fromJson(Map<String, dynamic> json) => _$IndentBlockFromJson(json);
}
@JsonSerializable()
class ScopedStatement extends Stmt {
    StructType? structType;
    Node? statement;
    Scope? scope;
factory ScopedStatement.fromJson(Map<String, dynamic> json) => _$ScopedStatementFromJson(json);
}
@JsonSerializable()
class MatchBranch extends Stmt {
    Match? belong;
    Identity? cond;
    Loc symLoc = Loc();
    Node? then;
factory MatchBranch.fromJson(Map<String, dynamic> json) => _$MatchBranchFromJson(json);
}
@JsonSerializable()
class UnionCandidate extends Stmt {
    Expr? cond;
    Field? field;
factory UnionCandidate.fromJson(Map<String, dynamic> json) => _$UnionCandidateFromJson(json);
}
@JsonSerializable()
class Return extends Stmt {
    Expr? expr;
    Func? relatedFunction;
factory Return.fromJson(Map<String, dynamic> json) => _$ReturnFromJson(json);
}
@JsonSerializable()
class Break extends Stmt {
    Loop? relatedLoop;
factory Break.fromJson(Map<String, dynamic> json) => _$BreakFromJson(json);
}
@JsonSerializable()
class Continue extends Stmt {
    Loop? relatedLoop;
factory Continue.fromJson(Map<String, dynamic> json) => _$ContinueFromJson(json);
}
@JsonSerializable()
class Assert extends Stmt {
    Binary? cond;
    bool isIoRelated = false;
factory Assert.fromJson(Map<String, dynamic> json) => _$AssertFromJson(json);
}
@JsonSerializable()
class ImplicitYield extends Stmt {
    Expr? expr;
factory ImplicitYield.fromJson(Map<String, dynamic> json) => _$ImplicitYieldFromJson(json);
}
@JsonSerializable()
class Metadata extends Stmt {
    Expr? base;
    String name = '';
    List<Expr>? values = [];
factory Metadata.fromJson(Map<String, dynamic> json) => _$MetadataFromJson(json);
}
@JsonSerializable()
class IntType extends Type {
    Endian endian = Endian.Unspec;
    bool isSigned = false;
    bool isCommonSupported = false;
factory IntType.fromJson(Map<String, dynamic> json) => _$IntTypeFromJson(json);
}
@JsonSerializable()
class FloatType extends Type {
    Endian endian = Endian.Unspec;
    bool isCommonSupported = false;
factory FloatType.fromJson(Map<String, dynamic> json) => _$FloatTypeFromJson(json);
}
@JsonSerializable()
class IdentType extends Type {
    MemberAccess? importRef;
    Ident? ident;
    Type? base;
factory IdentType.fromJson(Map<String, dynamic> json) => _$IdentTypeFromJson(json);
}
@JsonSerializable()
class IntLiteralType extends Type {
    IntLiteral? base;
factory IntLiteralType.fromJson(Map<String, dynamic> json) => _$IntLiteralTypeFromJson(json);
}
@JsonSerializable()
class StrLiteralType extends Type {
    StrLiteral? base;
    StrLiteral? strongRef;
factory StrLiteralType.fromJson(Map<String, dynamic> json) => _$StrLiteralTypeFromJson(json);
}
@JsonSerializable()
class RegexLiteralType extends Type {
    RegexLiteral? base;
    RegexLiteral? strongRef;
factory RegexLiteralType.fromJson(Map<String, dynamic> json) => _$RegexLiteralTypeFromJson(json);
}
@JsonSerializable()
class VoidType extends Type {
factory VoidType.fromJson(Map<String, dynamic> json) => _$VoidTypeFromJson(json);
}
@JsonSerializable()
class BoolType extends Type {
factory BoolType.fromJson(Map<String, dynamic> json) => _$BoolTypeFromJson(json);
}
@JsonSerializable()
class ArrayType extends Type {
    Loc endLoc = Loc();
    Type? elementType;
    Expr? length;
    int? lengthValue;
    bool isBytes = false;
factory ArrayType.fromJson(Map<String, dynamic> json) => _$ArrayTypeFromJson(json);
}
@JsonSerializable()
class FunctionType extends Type {
    Type? returnType;
    List<Type>? parameters = [];
factory FunctionType.fromJson(Map<String, dynamic> json) => _$FunctionTypeFromJson(json);
}
@JsonSerializable()
class StructType extends Type {
    List<Member>? fields = [];
    Node? base;
    bool recursive = false;
    int fixedHeaderSize = 0;
    int fixedTailSize = 0;
factory StructType.fromJson(Map<String, dynamic> json) => _$StructTypeFromJson(json);
}
@JsonSerializable()
class StructUnionType extends Type {
    Expr? cond;
    List<Expr>? conds = [];
    List<StructType>? structs = [];
    Expr? base;
    List<Field>? unionFields = [];
    bool exhaustive = false;
factory StructUnionType.fromJson(Map<String, dynamic> json) => _$StructUnionTypeFromJson(json);
}
@JsonSerializable()
class UnionType extends Type {
    Expr? cond;
    List<UnionCandidate>? candidates = [];
    StructUnionType? baseType;
    Type? commonType;
    List<Field>? memberCandidates = [];
factory UnionType.fromJson(Map<String, dynamic> json) => _$UnionTypeFromJson(json);
}
@JsonSerializable()
class RangeType extends Type {
    Type? baseType;
    Range? range;
factory RangeType.fromJson(Map<String, dynamic> json) => _$RangeTypeFromJson(json);
}
@JsonSerializable()
class EnumType extends Type {
    Enum? base;
factory EnumType.fromJson(Map<String, dynamic> json) => _$EnumTypeFromJson(json);
}
@JsonSerializable()
class MetaType extends Type {
factory MetaType.fromJson(Map<String, dynamic> json) => _$MetaTypeFromJson(json);
}
@JsonSerializable()
class OptionalType extends Type {
    Type? baseType;
factory OptionalType.fromJson(Map<String, dynamic> json) => _$OptionalTypeFromJson(json);
}
@JsonSerializable()
class GenericType extends Type {
    Member? belong;
factory GenericType.fromJson(Map<String, dynamic> json) => _$GenericTypeFromJson(json);
}
@JsonSerializable()
class IntLiteral extends Literal {
    String value = '';
factory IntLiteral.fromJson(Map<String, dynamic> json) => _$IntLiteralFromJson(json);
}
@JsonSerializable()
class BoolLiteral extends Literal {
    bool value = false;
factory BoolLiteral.fromJson(Map<String, dynamic> json) => _$BoolLiteralFromJson(json);
}
@JsonSerializable()
class StrLiteral extends Literal {
    String value = '';
    int length = 0;
factory StrLiteral.fromJson(Map<String, dynamic> json) => _$StrLiteralFromJson(json);
}
@JsonSerializable()
class RegexLiteral extends Literal {
    String value = '';
factory RegexLiteral.fromJson(Map<String, dynamic> json) => _$RegexLiteralFromJson(json);
}
@JsonSerializable()
class CharLiteral extends Literal {
    String value = '';
    int code = 0;
factory CharLiteral.fromJson(Map<String, dynamic> json) => _$CharLiteralFromJson(json);
}
@JsonSerializable()
class TypeLiteral extends Literal {
    Type? typeLiteral;
    Loc endLoc = Loc();
factory TypeLiteral.fromJson(Map<String, dynamic> json) => _$TypeLiteralFromJson(json);
}
@JsonSerializable()
class SpecialLiteral extends Literal {
    SpecialLiteralKind kind = SpecialLiteralKind.Input;
factory SpecialLiteral.fromJson(Map<String, dynamic> json) => _$SpecialLiteralFromJson(json);
}
@JsonSerializable()
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
factory Field.fromJson(Map<String, dynamic> json) => _$FieldFromJson(json);
}
@JsonSerializable()
class Format extends Member {
    IndentBlock? body;
    Func? encodeFn;
    Func? decodeFn;
    List<Func>? castFns = [];
    List<IdentType>? depends = [];
    List<Field>? stateVariables = [];
factory Format.fromJson(Map<String, dynamic> json) => _$FormatFromJson(json);
}
@JsonSerializable()
class State extends Member {
    IndentBlock? body;
factory State.fromJson(Map<String, dynamic> json) => _$StateFromJson(json);
}
@JsonSerializable()
class Enum extends Member {
    Scope? scope;
    Loc colonLoc = Loc();
    Type? baseType;
    List<EnumMember>? members = [];
    EnumType? enumType;
factory Enum.fromJson(Map<String, dynamic> json) => _$EnumFromJson(json);
}
@JsonSerializable()
class EnumMember extends Member {
    Expr? rawExpr;
    Expr? value;
    StrLiteral? strLiteral;
factory EnumMember.fromJson(Map<String, dynamic> json) => _$EnumMemberFromJson(json);
}
@JsonSerializable()
class Func extends Member {
    List<Field>? parameters = [];
    Type? returnType;
    IndentBlock? body;
    FunctionType? funcType;
    bool isCast = false;
factory Func.fromJson(Map<String, dynamic> json) => _$FuncFromJson(json);
}
@JsonSerializable()
class Scope {
    Scope? prev;
    Scope? next;
    Scope? branch;
    List<Ident>? ident = [];
    Node? owner;
    bool branchRoot = false;
factory Scope.fromJson(Map<String, dynamic> json) => _$ScopeFromJson(json);
}
@JsonSerializable()
class Pos {
    int begin = 0;
    int end = 0;
factory Pos.fromJson(Map<String, dynamic> json) => _$PosFromJson(json);
}
@JsonSerializable()
class Loc {
    Pos pos = Pos();
    int file = 0;
    int line = 0;
    int col = 0;
factory Loc.fromJson(Map<String, dynamic> json) => _$LocFromJson(json);
}
@JsonSerializable()
class Token {
    TokenTag tag = TokenTag.Indent;
    String token = '';
    Loc loc = Loc();
factory Token.fromJson(Map<String, dynamic> json) => _$TokenFromJson(json);
}
@JsonSerializable()
class RawScope {
    int? prev;
    int? next;
    int? branch;
    List<int> ident = [];
    int? owner;
    bool branchRoot = false;
factory RawScope.fromJson(Map<String, dynamic> json) => _$RawScopeFromJson(json);
}
@JsonSerializable()
class RawNode {
    NodeType nodeType = NodeType.Program;
    Loc loc = Loc();
    dynamic body;
factory RawNode.fromJson(Map<String, dynamic> json) => _$RawNodeFromJson(json);
}
@JsonSerializable()
class SrcErrorEntry {
    String msg = '';
    String file = '';
    Loc loc = Loc();
    String src = '';
    bool warn = false;
factory SrcErrorEntry.fromJson(Map<String, dynamic> json) => _$SrcErrorEntryFromJson(json);
}
@JsonSerializable()
class SrcError {
    List<SrcErrorEntry> errs = [];
factory SrcError.fromJson(Map<String, dynamic> json) => _$SrcErrorFromJson(json);
}
@JsonSerializable()
class JsonAst {
    List<RawNode> node = [];
    List<RawScope> scope = [];
factory JsonAst.fromJson(Map<String, dynamic> json) => _$JsonAstFromJson(json);
}
@JsonSerializable()
class AstFile {
    bool success = false;
    List<String> files = [];
    JsonAst? ast;
    SrcError? error;
factory AstFile.fromJson(Map<String, dynamic> json) => _$AstFileFromJson(json);
}
@JsonSerializable()
class TokenFile {
    bool success = false;
    List<String> files = [];
    List<Token>? tokens = [];
    SrcError? error;
factory TokenFile.fromJson(Map<String, dynamic> json) => _$TokenFileFromJson(json);
}
