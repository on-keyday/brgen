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
flowchart TB
NodeType[NodeType]
NodeType -->|member|program
NodeType -->|member|comment
NodeType -->|member|comment_group
NodeType -->|member|field_argument
NodeType -->|member|expr
NodeType -->|member|binary
NodeType -->|member|unary
NodeType -->|member|cond
NodeType -->|member|ident
NodeType -->|member|call_
NodeType -->|member|if
NodeType -->|member|member_access
NodeType -->|member|paren
NodeType -->|member|index
NodeType -->|member|match
NodeType -->|member|range
NodeType -->|member|tmp_var
NodeType -->|member|import
NodeType -->|member|cast
NodeType -->|member|available
NodeType -->|member|specify_endian
NodeType -->|member|explicit_error
NodeType -->|member|stmt
NodeType -->|member|loop
NodeType -->|member|indent_block
NodeType -->|member|scoped_statement
NodeType -->|member|match_branch
NodeType -->|member|union_candidate
NodeType -->|member|return
NodeType -->|member|break
NodeType -->|member|continue
NodeType -->|member|assert
NodeType -->|member|implicit_yield
NodeType -->|member|type
NodeType -->|member|int_type
NodeType -->|member|ident_type
NodeType -->|member|int_literal_type
NodeType -->|member|str_literal_type
NodeType -->|member|void_type
NodeType -->|member|bool_type
NodeType -->|member|array_type
NodeType -->|member|function_type
NodeType -->|member|struct_type
NodeType -->|member|struct_union_type
NodeType -->|member|union_type
NodeType -->|member|range_type
NodeType -->|member|enum_type
NodeType -->|member|literal
NodeType -->|member|int_literal
NodeType -->|member|bool_literal
NodeType -->|member|str_literal
NodeType -->|member|input
NodeType -->|member|output
NodeType -->|member|config
NodeType -->|member|member
NodeType -->|member|field
NodeType -->|member|format
NodeType -->|member|state
NodeType -->|member|enum
NodeType -->|member|enum_member
NodeType -->|member|function
NodeType -->|member|builtin_function
TokenTag[TokenTag]
TokenTag -->|member|indent
TokenTag -->|member|space
TokenTag -->|member|line
TokenTag -->|member|punct
TokenTag -->|member|int_literal
TokenTag -->|member|bool_literal
TokenTag -->|member|str_literal
TokenTag -->|member|keyword
TokenTag -->|member|ident
TokenTag -->|member|comment
TokenTag -->|member|error
TokenTag -->|member|unknown
UnaryOp[UnaryOp]
UnaryOp -->|member|not
UnaryOp -->|member|minus_sign
BinaryOp[BinaryOp]
BinaryOp -->|member|mul
BinaryOp -->|member|div
BinaryOp -->|member|mod
BinaryOp -->|member|left_arithmetic_shift
BinaryOp -->|member|right_arithmetic_shift
BinaryOp -->|member|left_logical_shift
BinaryOp -->|member|right_logical_shift
BinaryOp -->|member|bit_and
BinaryOp -->|member|add
BinaryOp -->|member|sub
BinaryOp -->|member|bit_or
BinaryOp -->|member|bit_xor
BinaryOp -->|member|equal
BinaryOp -->|member|not_equal
BinaryOp -->|member|less
BinaryOp -->|member|less_or_eq
BinaryOp -->|member|grater
BinaryOp -->|member|grater_or_eq
BinaryOp -->|member|logical_and
BinaryOp -->|member|logical_or
BinaryOp -->|member|cond_op_1
BinaryOp -->|member|cond_op_2
BinaryOp -->|member|range_exclusive
BinaryOp -->|member|range_inclusive
BinaryOp -->|member|assign
BinaryOp -->|member|define_assign
BinaryOp -->|member|const_assign
BinaryOp -->|member|add_assign
BinaryOp -->|member|sub_assign
BinaryOp -->|member|mul_assign
BinaryOp -->|member|div_assign
BinaryOp -->|member|mod_assign
BinaryOp -->|member|left_shift_assign
BinaryOp -->|member|right_shift_assign
BinaryOp -->|member|bit_and_assign
BinaryOp -->|member|bit_or_assign
BinaryOp -->|member|bit_xor_assign
BinaryOp -->|member|comma
IdentUsage[IdentUsage]
IdentUsage -->|member|unknown
IdentUsage -->|member|reference
IdentUsage -->|member|define_variable
IdentUsage -->|member|define_const
IdentUsage -->|member|define_field
IdentUsage -->|member|define_format
IdentUsage -->|member|define_state
IdentUsage -->|member|define_enum
IdentUsage -->|member|define_enum_member
IdentUsage -->|member|define_fn
IdentUsage -->|member|define_cast_fn
IdentUsage -->|member|define_arg
IdentUsage -->|member|reference_type
IdentUsage -->|member|reference_member
IdentUsage -->|member|maybe_type
IdentUsage -->|member|reference_builtin_fn
Endian[Endian]
Endian -->|member|unspec
Endian -->|member|big
Endian -->|member|little
ConstantLevel[ConstantLevel]
ConstantLevel -->|member|unknown
ConstantLevel -->|member|constant
ConstantLevel -->|member|const_variable
ConstantLevel -->|member|variable
BitAlignment[BitAlignment]
BitAlignment -->|member|byte_aligned
BitAlignment -->|member|bit_1
BitAlignment -->|member|bit_2
BitAlignment -->|member|bit_3
BitAlignment -->|member|bit_4
BitAlignment -->|member|bit_5
BitAlignment -->|member|bit_6
BitAlignment -->|member|bit_7
BitAlignment -->|member|not_target
BitAlignment -->|member|not_decidable
Follow[Follow]
Follow -->|member|unknown
Follow -->|member|end_
Follow -->|member|fixed
Follow -->|member|constant
Follow -->|member|normal
Node[Node]
Node -->|member|loc
loc -->|type|Loc
Expr[Expr]
Node -->|derive|Expr
Expr -->|member|expr_type
expr_type -->|type|Type
Expr -->|member|constant_level
constant_level -->|type|ConstantLevel
Stmt[Stmt]
Node -->|derive|Stmt
Type[Type]
Node -->|derive|Type
Type -->|member|is_explicit
is_explicit -->|type|boolean
Type -->|member|is_int_set
is_int_set -->|type|boolean
Type -->|member|bit_alignment
bit_alignment -->|type|BitAlignment
Type -->|member|bit_size
bit_size -->|type|number
Literal[Literal]
Expr -->|derive|Literal
Member[Member]
Stmt -->|derive|Member
Member -->|member|belong
belong -->|type|Member
Member -->|member|belong_struct
belong_struct -->|type|StructType
Member -->|member|ident
ident -->|type|Ident
Program[Program]
Node -->|derive|Program
Program -->|member|struct_type
struct_type -->|type|StructType
Program -->|member|elements
elements -->|type|Node
Program -->|member|global_scope
global_scope -->|type|Scope
Comment[Comment]
Node -->|derive|Comment
Comment -->|member|comment
comment -->|type|string
CommentGroup[CommentGroup]
Node -->|derive|CommentGroup
CommentGroup -->|member|comments
comments -->|type|Comment
FieldArgument[FieldArgument]
Node -->|derive|FieldArgument
FieldArgument -->|member|raw_arguments
raw_arguments -->|type|Expr
FieldArgument -->|member|end_loc
end_loc -->|type|Loc
FieldArgument -->|member|collected_arguments
collected_arguments -->|type|Expr
FieldArgument -->|member|arguments
arguments -->|type|Expr
FieldArgument -->|member|alignment
alignment -->|type|Expr
FieldArgument -->|member|alignment_value
alignment_value -->|type|number
FieldArgument -->|member|sub_byte_length
sub_byte_length -->|type|Expr
FieldArgument -->|member|sub_byte_begin
sub_byte_begin -->|type|Expr
Binary[Binary]
Expr -->|derive|Binary
Binary -->|member|op
op -->|type|BinaryOp
Binary -->|member|left
left -->|type|Expr
Binary -->|member|right
right -->|type|Expr
Unary[Unary]
Expr -->|derive|Unary
Unary -->|member|op
op -->|type|UnaryOp
Unary -->|member|expr
expr -->|type|Expr
Cond[Cond]
Expr -->|derive|Cond
Cond -->|member|cond
cond -->|type|Expr
Cond -->|member|then
then -->|type|Expr
Cond -->|member|els_loc
els_loc -->|type|Loc
Cond -->|member|els
els -->|type|Expr
Ident[Ident]
Expr -->|derive|Ident
Ident -->|member|ident
ident -->|type|string
Ident -->|member|usage
usage -->|type|IdentUsage
Ident -->|member|base
base -->|type|Node
Ident -->|member|scope
scope -->|type|Scope
Call[Call]
Expr -->|derive|Call
Call -->|member|callee
callee -->|type|Expr
Call -->|member|raw_arguments
raw_arguments -->|type|Expr
Call -->|member|arguments
arguments -->|type|Expr
Call -->|member|end_loc
end_loc -->|type|Loc
If[If]
Expr -->|derive|If
If -->|member|cond_scope
cond_scope -->|type|Scope
If -->|member|cond
cond -->|type|Expr
If -->|member|then
then -->|type|IndentBlock
If -->|member|els
els -->|type|Node
MemberAccess[MemberAccess]
Expr -->|derive|MemberAccess
MemberAccess -->|member|target
target -->|type|Expr
MemberAccess -->|member|member
member -->|type|Ident
MemberAccess -->|member|base
base -->|type|Node
Paren[Paren]
Expr -->|derive|Paren
Paren -->|member|expr
expr -->|type|Expr
Paren -->|member|end_loc
end_loc -->|type|Loc
Index[Index]
Expr -->|derive|Index
Index -->|member|expr
expr -->|type|Expr
Index -->|member|index
index -->|type|Expr
Index -->|member|end_loc
end_loc -->|type|Loc
Match[Match]
Expr -->|derive|Match
Match -->|member|cond_scope
cond_scope -->|type|Scope
Match -->|member|cond
cond -->|type|Expr
Match -->|member|branch
branch -->|type|Node
Range[Range]
Expr -->|derive|Range
Range -->|member|op
op -->|type|BinaryOp
Range -->|member|start
start -->|type|Expr
Range -->|member|end
end -->|type|Expr
TmpVar[TmpVar]
Expr -->|derive|TmpVar
TmpVar -->|member|tmp_var
tmp_var -->|type|number
Import[Import]
Expr -->|derive|Import
Import -->|member|path
path -->|type|string
Import -->|member|base
base -->|type|Call
Import -->|member|import_desc
import_desc -->|type|Program
Cast[Cast]
Expr -->|derive|Cast
Cast -->|member|base
base -->|type|Call
Cast -->|member|expr
expr -->|type|Expr
Available[Available]
Expr -->|derive|Available
Available -->|member|base
base -->|type|Call
Available -->|member|target
target -->|type|Expr
SpecifyEndian[SpecifyEndian]
Expr -->|derive|SpecifyEndian
SpecifyEndian -->|member|base
base -->|type|Binary
SpecifyEndian -->|member|is_little
is_little -->|type|Expr
ExplicitError[ExplicitError]
Expr -->|derive|ExplicitError
ExplicitError -->|member|base
base -->|type|Call
ExplicitError -->|member|message
message -->|type|StrLiteral
Loop[Loop]
Stmt -->|derive|Loop
Loop -->|member|cond_scope
cond_scope -->|type|Scope
Loop -->|member|init
init -->|type|Expr
Loop -->|member|cond
cond -->|type|Expr
Loop -->|member|step
step -->|type|Expr
Loop -->|member|body
body -->|type|IndentBlock
IndentBlock[IndentBlock]
Stmt -->|derive|IndentBlock
IndentBlock -->|member|struct_type
struct_type -->|type|StructType
IndentBlock -->|member|elements
elements -->|type|Node
IndentBlock -->|member|scope
scope -->|type|Scope
ScopedStatement[ScopedStatement]
Stmt -->|derive|ScopedStatement
ScopedStatement -->|member|struct_type
struct_type -->|type|StructType
ScopedStatement -->|member|statement
statement -->|type|Node
ScopedStatement -->|member|scope
scope -->|type|Scope
MatchBranch[MatchBranch]
Stmt -->|derive|MatchBranch
MatchBranch -->|member|cond
cond -->|type|Expr
MatchBranch -->|member|sym_loc
sym_loc -->|type|Loc
MatchBranch -->|member|then
then -->|type|Node
UnionCandidate[UnionCandidate]
Stmt -->|derive|UnionCandidate
UnionCandidate -->|member|cond
cond -->|type|Expr
UnionCandidate -->|member|field
field -->|type|Field
Return[Return]
Stmt -->|derive|Return
Return -->|member|expr
expr -->|type|Expr
Break[Break]
Stmt -->|derive|Break
Continue[Continue]
Stmt -->|derive|Continue
Assert[Assert]
Stmt -->|derive|Assert
Assert -->|member|cond
cond -->|type|Binary
ImplicitYield[ImplicitYield]
Stmt -->|derive|ImplicitYield
ImplicitYield -->|member|expr
expr -->|type|Expr
IntType[IntType]
Type -->|derive|IntType
IntType -->|member|endian
endian -->|type|Endian
IntType -->|member|is_signed
is_signed -->|type|boolean
IntType -->|member|is_common_supported
is_common_supported -->|type|boolean
IdentType[IdentType]
Type -->|derive|IdentType
IdentType -->|member|ident
ident -->|type|Ident
IdentType -->|member|base
base -->|type|Type
IntLiteralType[IntLiteralType]
Type -->|derive|IntLiteralType
IntLiteralType -->|member|base
base -->|type|IntLiteral
StrLiteralType[StrLiteralType]
Type -->|derive|StrLiteralType
StrLiteralType -->|member|base
base -->|type|StrLiteral
StrLiteralType -->|member|strong_ref
strong_ref -->|type|StrLiteral
VoidType[VoidType]
Type -->|derive|VoidType
BoolType[BoolType]
Type -->|derive|BoolType
ArrayType[ArrayType]
Type -->|derive|ArrayType
ArrayType -->|member|end_loc
end_loc -->|type|Loc
ArrayType -->|member|base_type
base_type -->|type|Type
ArrayType -->|member|length
length -->|type|Expr
ArrayType -->|member|length_value
length_value -->|type|number
FunctionType[FunctionType]
Type -->|derive|FunctionType
FunctionType -->|member|return_type
return_type -->|type|Type
FunctionType -->|member|parameters
parameters -->|type|Type
StructType[StructType]
Type -->|derive|StructType
StructType -->|member|fields
fields -->|type|Member
StructType -->|member|base
base -->|type|Node
StructType -->|member|recursive
recursive -->|type|boolean
StructUnionType[StructUnionType]
Type -->|derive|StructUnionType
StructUnionType -->|member|structs
structs -->|type|StructType
StructUnionType -->|member|base
base -->|type|Expr
StructUnionType -->|member|union_fields
union_fields -->|type|Field
UnionType[UnionType]
Type -->|derive|UnionType
UnionType -->|member|cond
cond -->|type|Expr
UnionType -->|member|candidates
candidates -->|type|UnionCandidate
UnionType -->|member|base_type
base_type -->|type|StructUnionType
UnionType -->|member|common_type
common_type -->|type|Type
RangeType[RangeType]
Type -->|derive|RangeType
RangeType -->|member|base_type
base_type -->|type|Type
RangeType -->|member|range
range -->|type|Range
EnumType[EnumType]
Type -->|derive|EnumType
EnumType -->|member|base
base -->|type|Enum
IntLiteral[IntLiteral]
Literal -->|derive|IntLiteral
IntLiteral -->|member|value
value -->|type|string
BoolLiteral[BoolLiteral]
Literal -->|derive|BoolLiteral
BoolLiteral -->|member|value
value -->|type|boolean
StrLiteral[StrLiteral]
Literal -->|derive|StrLiteral
StrLiteral -->|member|value
value -->|type|string
StrLiteral -->|member|length
length -->|type|number
Input[Input]
Literal -->|derive|Input
Output[Output]
Literal -->|derive|Output
Config[Config]
Literal -->|derive|Config
Field[Field]
Member -->|derive|Field
Field -->|member|colon_loc
colon_loc -->|type|Loc
Field -->|member|field_type
field_type -->|type|Type
Field -->|member|arguments
arguments -->|type|FieldArgument
Field -->|member|bit_alignment
bit_alignment -->|type|BitAlignment
Field -->|member|follow
follow -->|type|Follow
Field -->|member|eventual_follow
eventual_follow -->|type|Follow
Format[Format]
Member -->|derive|Format
Format -->|member|body
body -->|type|IndentBlock
Format -->|member|encode_fn
encode_fn -->|type|Function
Format -->|member|decode_fn
decode_fn -->|type|Function
Format -->|member|cast_fns
cast_fns -->|type|Function
State[State]
Member -->|derive|State
State -->|member|body
body -->|type|IndentBlock
Enum[Enum]
Member -->|derive|Enum
Enum -->|member|scope
scope -->|type|Scope
Enum -->|member|colon_loc
colon_loc -->|type|Loc
Enum -->|member|base_type
base_type -->|type|Type
Enum -->|member|members
members -->|type|EnumMember
Enum -->|member|enum_type
enum_type -->|type|EnumType
EnumMember[EnumMember]
Member -->|derive|EnumMember
EnumMember -->|member|expr
expr -->|type|Expr
Function[Function]
Member -->|derive|Function
Function -->|member|parameters
parameters -->|type|Field
Function -->|member|return_type
return_type -->|type|Type
Function -->|member|body
body -->|type|IndentBlock
Function -->|member|func_type
func_type -->|type|FunctionType
Function -->|member|is_cast
is_cast -->|type|boolean
Function -->|member|cast_loc
cast_loc -->|type|Loc
BuiltinFunction[BuiltinFunction]
Member -->|derive|BuiltinFunction
BuiltinFunction -->|member|func_type
func_type -->|type|FunctionType
Scope[Scope]
Scope -->|member|prev
prev -->|type|Scope
Scope -->|member|next
next -->|type|Scope
Scope -->|member|branch
branch -->|type|Scope
Scope -->|member|ident
ident -->|type|Ident
Scope -->|member|owner
owner -->|type|Node
Scope -->|member|branch_root
branch_root -->|type|boolean
Pos[Pos]
Pos -->|member|begin
begin -->|type|number
Pos -->|member|end
end -->|type|number
Loc[Loc]
Loc -->|member|pos
pos -->|type|Pos
Loc -->|member|file
file -->|type|number
Loc -->|member|line
line -->|type|number
Loc -->|member|col
col -->|type|number
Token[Token]
Token -->|member|tag
tag -->|type|TokenTag
Token -->|member|token
token -->|type|string
Token -->|member|loc
loc -->|type|Loc
RawScope[RawScope]
RawScope -->|member|prev
prev -->|type|number
RawScope -->|member|next
next -->|type|number
RawScope -->|member|branch
branch -->|type|number
RawScope -->|member|ident
ident -->|type|number
RawScope -->|member|owner
owner -->|type|number
RawScope -->|member|branch_root
branch_root -->|type|boolean
RawNode[RawNode]
RawNode -->|member|node_type
node_type -->|type|NodeType
RawNode -->|member|loc
loc -->|type|Loc
RawNode -->|member|body
body -->|type|any
SrcErrorEntry[SrcErrorEntry]
SrcErrorEntry -->|member|msg
msg -->|type|string
SrcErrorEntry -->|member|file
file -->|type|string
SrcErrorEntry -->|member|loc
loc -->|type|Loc
SrcErrorEntry -->|member|src
src -->|type|string
SrcErrorEntry -->|member|warn
warn -->|type|boolean
SrcError[SrcError]
SrcError -->|member|errs
errs -->|type|SrcErrorEntry
JsonAst[JsonAst]
JsonAst -->|member|node
node -->|type|RawNode
JsonAst -->|member|scope
scope -->|type|RawScope
AstFile[AstFile]
AstFile -->|member|files
files -->|type|string
AstFile -->|member|ast
ast -->|type|JsonAst
AstFile -->|member|error
error -->|type|SrcError
TokenFile[TokenFile]
TokenFile -->|member|files
files -->|type|string
TokenFile -->|member|tokens
tokens -->|type|Token
TokenFile -->|member|error
error -->|type|SrcError
```

{{< mermaid >}}
