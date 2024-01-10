---
title: "Ast"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# AST

brgen の AST を記述する
TODO(on-keyday): 自動生成する?

```mermaid
erDiagram
NodeType {
NodeType program
NodeType comment
NodeType comment_group
NodeType field_argument
NodeType expr
NodeType binary
NodeType unary
NodeType cond
NodeType ident
NodeType call_
NodeType if
NodeType member_access
NodeType paren
NodeType index
NodeType match
NodeType range
NodeType tmp_var
NodeType import
NodeType cast
NodeType available
NodeType specify_endian
NodeType explicit_error
NodeType stmt
NodeType loop
NodeType indent_block
NodeType scoped_statement
NodeType match_branch
NodeType union_candidate
NodeType return
NodeType break
NodeType continue
NodeType assert
NodeType implicit_yield
NodeType type
NodeType int_type
NodeType ident_type
NodeType int_literal_type
NodeType str_literal_type
NodeType void_type
NodeType bool_type
NodeType array_type
NodeType function_type
NodeType struct_type
NodeType struct_union_type
NodeType union_type
NodeType range_type
NodeType enum_type
NodeType literal
NodeType int_literal
NodeType bool_literal
NodeType str_literal
NodeType input
NodeType output
NodeType config
NodeType member
NodeType field
NodeType format
NodeType state
NodeType enum
NodeType enum_member
NodeType function
NodeType builtin_function
}
TokenTag {
TokenTag indent
TokenTag space
TokenTag line
TokenTag punct
TokenTag int_literal
TokenTag bool_literal
TokenTag str_literal
TokenTag keyword
TokenTag ident
TokenTag comment
TokenTag error
TokenTag unknown
}
UnaryOp {
UnaryOp not
UnaryOp minus_sign
}
BinaryOp {
BinaryOp mul
BinaryOp div
BinaryOp mod
BinaryOp left_arithmetic_shift
BinaryOp right_arithmetic_shift
BinaryOp left_logical_shift
BinaryOp right_logical_shift
BinaryOp bit_and
BinaryOp add
BinaryOp sub
BinaryOp bit_or
BinaryOp bit_xor
BinaryOp equal
BinaryOp not_equal
BinaryOp less
BinaryOp less_or_eq
BinaryOp grater
BinaryOp grater_or_eq
BinaryOp logical_and
BinaryOp logical_or
BinaryOp cond_op_1
BinaryOp cond_op_2
BinaryOp range_exclusive
BinaryOp range_inclusive
BinaryOp assign
BinaryOp define_assign
BinaryOp const_assign
BinaryOp add_assign
BinaryOp sub_assign
BinaryOp mul_assign
BinaryOp div_assign
BinaryOp mod_assign
BinaryOp left_shift_assign
BinaryOp right_shift_assign
BinaryOp bit_and_assign
BinaryOp bit_or_assign
BinaryOp bit_xor_assign
BinaryOp comma
}
IdentUsage {
IdentUsage unknown
IdentUsage reference
IdentUsage define_variable
IdentUsage define_const
IdentUsage define_field
IdentUsage define_format
IdentUsage define_state
IdentUsage define_enum
IdentUsage define_enum_member
IdentUsage define_fn
IdentUsage define_cast_fn
IdentUsage define_arg
IdentUsage reference_type
IdentUsage reference_member
IdentUsage maybe_type
IdentUsage reference_builtin_fn
}
Endian {
Endian unspec
Endian big
Endian little
}
ConstantLevel {
ConstantLevel unknown
ConstantLevel constant
ConstantLevel const_variable
ConstantLevel variable
}
BitAlignment {
BitAlignment byte_aligned
BitAlignment bit_1
BitAlignment bit_2
BitAlignment bit_3
BitAlignment bit_4
BitAlignment bit_5
BitAlignment bit_6
BitAlignment bit_7
BitAlignment not_target
BitAlignment not_decidable
}
Follow {
Follow unknown
Follow end_
Follow fixed
Follow constant
Follow normal
}
Node {
Loc Loc
}
Node ||--|| Loc : strong
Expr {
Type Type
ConstantLevel ConstantLevel
}
Node ||--|| Expr : derive
Expr ||--|| Type : strong
Expr ||--|| ConstantLevel : strong
Stmt {
}
Node ||--|| Stmt : derive
Type {
boolean boolean
boolean boolean
BitAlignment BitAlignment
number number
}
Node ||--|| Type : derive
Type ||--|| boolean : strong
Type ||--|| boolean : strong
Type ||--|| BitAlignment : strong
Type ||--|| number : strong
Literal {
}
Expr ||--|| Literal : derive
Member {
Member Member
StructType StructType
Ident Ident
}
Stmt ||--|| Member : derive
Member ||--|| Member : weak
Member ||--|| StructType : weak
Member ||--|| Ident : strong
Node ||--|| Program : derive
Program {
StructType struct_type
Node elements
Scope global_scope
}
Program ||--||StructType : strong
Program ||--||Node : strong
Program ||--||Scope : strong
Node ||--|| Comment : derive
Comment {
string comment
}
Comment ||--||string : strong
Node ||--|| CommentGroup : derive
CommentGroup {
Comment comments
}
CommentGroup ||--||Comment : strong
Node ||--|| FieldArgument : derive
FieldArgument {
Expr raw_arguments
Loc end_loc
Expr collected_arguments
Expr arguments
Expr alignment
number alignment_value
Expr sub_byte_length
Expr sub_byte_begin
}
FieldArgument ||--||Expr : strong
FieldArgument ||--||Loc : strong
FieldArgument ||--||Expr : weak
FieldArgument ||--||Expr : strong
FieldArgument ||--||Expr : strong
FieldArgument ||--||number : strong
FieldArgument ||--||Expr : strong
FieldArgument ||--||Expr : strong
Expr ||--|| Binary : derive
Binary {
BinaryOp op
Expr left
Expr right
}
Binary ||--||BinaryOp : strong
Binary ||--||Expr : strong
Binary ||--||Expr : strong
Expr ||--|| Unary : derive
Unary {
UnaryOp op
Expr expr
}
Unary ||--||UnaryOp : strong
Unary ||--||Expr : strong
Expr ||--|| Cond : derive
Cond {
Expr cond
Expr then
Loc els_loc
Expr els
}
Cond ||--||Expr : strong
Cond ||--||Expr : strong
Cond ||--||Loc : strong
Cond ||--||Expr : strong
Expr ||--|| Ident : derive
Ident {
string ident
IdentUsage usage
Node base
Scope scope
}
Ident ||--||string : strong
Ident ||--||IdentUsage : strong
Ident ||--||Node : weak
Ident ||--||Scope : strong
Expr ||--|| Call : derive
Call {
Expr callee
Expr raw_arguments
Expr arguments
Loc end_loc
}
Call ||--||Expr : strong
Call ||--||Expr : strong
Call ||--||Expr : strong
Call ||--||Loc : strong
Expr ||--|| If : derive
If {
Scope cond_scope
Expr cond
IndentBlock then
Node els
}
If ||--||Scope : strong
If ||--||Expr : strong
If ||--||IndentBlock : strong
If ||--||Node : strong
Expr ||--|| MemberAccess : derive
MemberAccess {
Expr target
Ident member
Node base
}
MemberAccess ||--||Expr : strong
MemberAccess ||--||Ident : strong
MemberAccess ||--||Node : weak
Expr ||--|| Paren : derive
Paren {
Expr expr
Loc end_loc
}
Paren ||--||Expr : strong
Paren ||--||Loc : strong
Expr ||--|| Index : derive
Index {
Expr expr
Expr index
Loc end_loc
}
Index ||--||Expr : strong
Index ||--||Expr : strong
Index ||--||Loc : strong
Expr ||--|| Match : derive
Match {
Scope cond_scope
Expr cond
Node branch
}
Match ||--||Scope : strong
Match ||--||Expr : strong
Match ||--||Node : strong
Expr ||--|| Range : derive
Range {
BinaryOp op
Expr start
Expr end
}
Range ||--||BinaryOp : strong
Range ||--||Expr : strong
Range ||--||Expr : strong
Expr ||--|| TmpVar : derive
TmpVar {
number tmp_var
}
TmpVar ||--||number : strong
Expr ||--|| Import : derive
Import {
string path
Call base
Program import_desc
}
Import ||--||string : strong
Import ||--||Call : strong
Import ||--||Program : strong
Expr ||--|| Cast : derive
Cast {
Call base
Expr expr
}
Cast ||--||Call : strong
Cast ||--||Expr : strong
Expr ||--|| Available : derive
Available {
Call base
Expr target
}
Available ||--||Call : strong
Available ||--||Expr : strong
Expr ||--|| SpecifyEndian : derive
SpecifyEndian {
Binary base
Expr is_little
}
SpecifyEndian ||--||Binary : strong
SpecifyEndian ||--||Expr : strong
Expr ||--|| ExplicitError : derive
ExplicitError {
Call base
StrLiteral message
}
ExplicitError ||--||Call : strong
ExplicitError ||--||StrLiteral : strong
Stmt ||--|| Loop : derive
Loop {
Scope cond_scope
Expr init
Expr cond
Expr step
IndentBlock body
}
Loop ||--||Scope : strong
Loop ||--||Expr : strong
Loop ||--||Expr : strong
Loop ||--||Expr : strong
Loop ||--||IndentBlock : strong
Stmt ||--|| IndentBlock : derive
IndentBlock {
StructType struct_type
Node elements
Scope scope
}
IndentBlock ||--||StructType : strong
IndentBlock ||--||Node : strong
IndentBlock ||--||Scope : strong
Stmt ||--|| ScopedStatement : derive
ScopedStatement {
StructType struct_type
Node statement
Scope scope
}
ScopedStatement ||--||StructType : strong
ScopedStatement ||--||Node : strong
ScopedStatement ||--||Scope : strong
Stmt ||--|| MatchBranch : derive
MatchBranch {
Expr cond
Loc sym_loc
Node then
}
MatchBranch ||--||Expr : strong
MatchBranch ||--||Loc : strong
MatchBranch ||--||Node : strong
Stmt ||--|| UnionCandidate : derive
UnionCandidate {
Expr cond
Field field
}
UnionCandidate ||--||Expr : weak
UnionCandidate ||--||Field : weak
Stmt ||--|| Return : derive
Return {
Expr expr
}
Return ||--||Expr : strong
Stmt ||--|| Break : derive
Break {
}
Stmt ||--|| Continue : derive
Continue {
}
Stmt ||--|| Assert : derive
Assert {
Binary cond
}
Assert ||--||Binary : strong
Stmt ||--|| ImplicitYield : derive
ImplicitYield {
Expr expr
}
ImplicitYield ||--||Expr : strong
Type ||--|| IntType : derive
IntType {
Endian endian
boolean is_signed
boolean is_common_supported
}
IntType ||--||Endian : strong
IntType ||--||boolean : strong
IntType ||--||boolean : strong
Type ||--|| IdentType : derive
IdentType {
Ident ident
Type base
}
IdentType ||--||Ident : strong
IdentType ||--||Type : weak
Type ||--|| IntLiteralType : derive
IntLiteralType {
IntLiteral base
}
IntLiteralType ||--||IntLiteral : weak
Type ||--|| StrLiteralType : derive
StrLiteralType {
StrLiteral base
StrLiteral strong_ref
}
StrLiteralType ||--||StrLiteral : weak
StrLiteralType ||--||StrLiteral : strong
Type ||--|| VoidType : derive
VoidType {
}
Type ||--|| BoolType : derive
BoolType {
}
Type ||--|| ArrayType : derive
ArrayType {
Loc end_loc
Type base_type
Expr length
number length_value
}
ArrayType ||--||Loc : strong
ArrayType ||--||Type : strong
ArrayType ||--||Expr : strong
ArrayType ||--||number : strong
Type ||--|| FunctionType : derive
FunctionType {
Type return_type
Type parameters
}
FunctionType ||--||Type : strong
FunctionType ||--||Type : strong
Type ||--|| StructType : derive
StructType {
Member fields
Node base
boolean recursive
}
StructType ||--||Member : strong
StructType ||--||Node : weak
StructType ||--||boolean : strong
Type ||--|| StructUnionType : derive
StructUnionType {
StructType structs
Expr base
Field union_fields
}
StructUnionType ||--||StructType : strong
StructUnionType ||--||Expr : weak
StructUnionType ||--||Field : weak
Type ||--|| UnionType : derive
UnionType {
Expr cond
UnionCandidate candidates
StructUnionType base_type
Type common_type
}
UnionType ||--||Expr : weak
UnionType ||--||UnionCandidate : strong
UnionType ||--||StructUnionType : weak
UnionType ||--||Type : strong
Type ||--|| RangeType : derive
RangeType {
Type base_type
Range range
}
RangeType ||--||Type : strong
RangeType ||--||Range : weak
Type ||--|| EnumType : derive
EnumType {
Enum base
}
EnumType ||--||Enum : weak
Literal ||--|| IntLiteral : derive
IntLiteral {
string value
}
IntLiteral ||--||string : strong
Literal ||--|| BoolLiteral : derive
BoolLiteral {
boolean value
}
BoolLiteral ||--||boolean : strong
Literal ||--|| StrLiteral : derive
StrLiteral {
string value
number length
}
StrLiteral ||--||string : strong
StrLiteral ||--||number : strong
Literal ||--|| Input : derive
Input {
}
Literal ||--|| Output : derive
Output {
}
Literal ||--|| Config : derive
Config {
}
Member ||--|| Field : derive
Field {
Loc colon_loc
Type field_type
FieldArgument arguments
BitAlignment bit_alignment
Follow follow
Follow eventual_follow
}
Field ||--||Loc : strong
Field ||--||Type : strong
Field ||--||FieldArgument : strong
Field ||--||BitAlignment : strong
Field ||--||Follow : strong
Field ||--||Follow : strong
Member ||--|| Format : derive
Format {
IndentBlock body
Function encode_fn
Function decode_fn
Function cast_fns
}
Format ||--||IndentBlock : strong
Format ||--||Function : weak
Format ||--||Function : weak
Format ||--||Function : weak
Member ||--|| State : derive
State {
IndentBlock body
}
State ||--||IndentBlock : strong
Member ||--|| Enum : derive
Enum {
Scope scope
Loc colon_loc
Type base_type
EnumMember members
EnumType enum_type
}
Enum ||--||Scope : strong
Enum ||--||Loc : strong
Enum ||--||Type : strong
Enum ||--||EnumMember : strong
Enum ||--||EnumType : strong
Member ||--|| EnumMember : derive
EnumMember {
Expr expr
}
EnumMember ||--||Expr : strong
Member ||--|| Function : derive
Function {
Field parameters
Type return_type
IndentBlock body
FunctionType func_type
boolean is_cast
Loc cast_loc
}
Function ||--||Field : strong
Function ||--||Type : strong
Function ||--||IndentBlock : strong
Function ||--||FunctionType : strong
Function ||--||boolean : strong
Function ||--||Loc : strong
Member ||--|| BuiltinFunction : derive
BuiltinFunction {
FunctionType func_type
}
BuiltinFunction ||--||FunctionType : strong
Scope {
Scope prev
Scope next
Scope branch
Ident ident
Node owner
boolean branch_root
}
Scope ||--||Scope : weak
Scope ||--||Scope : strong
Scope ||--||Scope : strong
Scope ||--||Ident : weak
Scope ||--||Node : weak
Scope ||--||boolean : strong
Pos {
number begin
number end
}
Pos ||--||number : strong
Pos ||--||number : strong
Loc {
Pos pos
number file
number line
number col
}
Loc ||--||Pos : strong
Loc ||--||number : strong
Loc ||--||number : strong
Loc ||--||number : strong
Token {
TokenTag tag
string token
Loc loc
}
Token ||--||TokenTag : strong
Token ||--||string : strong
Token ||--||Loc : strong
RawScope {
number prev
number next
number branch
number ident
number owner
boolean branch_root
}
RawScope ||--||number : strong
RawScope ||--||number : strong
RawScope ||--||number : strong
RawScope ||--||number : strong
RawScope ||--||number : strong
RawScope ||--||boolean : strong
RawNode {
NodeType node_type
Loc loc
any body
}
RawNode ||--||NodeType : strong
RawNode ||--||Loc : strong
RawNode ||--||any : strong
SrcErrorEntry {
string msg
string file
Loc loc
string src
boolean warn
}
SrcErrorEntry ||--||string : strong
SrcErrorEntry ||--||string : strong
SrcErrorEntry ||--||Loc : strong
SrcErrorEntry ||--||string : strong
SrcErrorEntry ||--||boolean : strong
SrcError {
SrcErrorEntry errs
}
SrcError ||--||SrcErrorEntry : strong
JsonAst {
RawNode node
RawScope scope
}
JsonAst ||--||RawNode : strong
JsonAst ||--||RawScope : strong
AstFile {
string files
JsonAst ast
SrcError error
}
AstFile ||--||string : strong
AstFile ||--||JsonAst : strong
AstFile ||--||SrcError : strong
TokenFile {
string files
Token tokens
SrcError error
}
TokenFile ||--||string : strong
TokenFile ||--||Token : strong
TokenFile ||--||SrcError : strong
```

{{< mermaid >}}
