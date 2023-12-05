# brgen language specialization

#####

##### draft version 3

##### lang-version: 0.0.0.d3 # draft 3

##### author: on-keyday (https://github.com/on-keyday)

##### note: this is implementation guidelines for my first parser and generator

## 1. Introduction

goal of this language is simple/powerful/portable DSL for binary encoder/decoder/validator/logger/document etc... generator
simple - easy to write and read
powerful - enough to represent network protocol/file format
portable - write once, generate in any language

this document are example of usage and it's parsable by brgen

```py
# this is comment
```

## 2. Word Definition

GID - generator implementation defined - decided by generator implementer
MUST - generator implementer must implement that

## 3. Scope and Syntax

in global scope, there are format,enum,and import
in local scope, there are format,enum,import,field definition statement,loop statement,expressions (including if,match and assertion)
field definition statement and assignment expression makes identifier definition in it scope
format and enum makes type definition in it scope

in AST level, any of above can be written in anywhere but exclude above can be omitted by generator
usage of optional statements and expressions are GID but some attribute explained after are recommended to implement

### 3.1 Identifier(variable) lookup rule

identifier definition lookup rule are:

1.  if in local scope, search variable defined before in same scope,
    1.a if found, link variable with definition location
    1.b if not found then up the scope then start lookup from 1.
2.  if in global scope, then look up from first of definition to end

### 3.2 Type lookup rule

1.  look up from first of definition to end in scope (no difference between local and global)
2.  if not found, then up the

## 4. Literal and Keywords

```py
# boolean literal
true || false
# integer literal
1 == 3
# floating point literal if GID
1.0 - 1.0
# string literal
"Hello World"
```

are supported

### 4.1. Input and Output, Config

there are three special literal; input, output, and env

```py
input # means decoder input
output # means encoder output
config # means environment and configuration of generator
```

you can access these objects anywhere
some situation, access to these object makes separation of encoding and decoding
these keywords are represented as ident
these have some method or property, explained in
input
output
config

### 4.2 Keywords

keywords are:

```py
true
false
input
output
config
# above are treated as constant value
format
enum
fn
if
elif
else
match
loop
return
break
continue
uN,sN,ubN,sbN,ulN,slN # N MUST be grater than 0 integer
```

## 5. Expressions

expressions are used for several purpose, assignment,assertion, condition of if expr, length of VLA
expressions are finally evaluated as bool,int,float,string, custom enum type,or custom format type.
it is GID that evaluation of format type and float; if not supported, it should be evaluated as void

### 5.1 Binary Operator

```py
# +  addition
1 + 2
"a"+"b" # strings are allowed
# -  subtraction
1 - 2
# * multiplication
1 * 2
# / division
1 / 2
# % modulo
1 % 2
# & bit and
1 & 0x7 # hexadecimal support
# | bit or
1 | 2
# ^ bit xor
1 ^ 2
# >> logical right bit shift
2 >> 1
# << logical left bit shift
1 << 2
# >>> arithmetic right bit shift
2 >>> 1
# <<< arithmetic left bit shift
1 <<< 2
# logical operator finally evaluated as bool
# == equal
1 == 2
# != not equal
# 1 != 2
# < less
1 < 2
# > greater
1 > 2
# >= greater or equal
1 >= 2
# <= less or equal
1 <= 2
# range exclusive
1..2
# range inclusive
1..=2
# range exclusive (right unlimited)
1..
# range exclusive (left unlimited)
..2
# range unlimited
..
```

note that in Java language, arithmetic shift are `<<` and `>>` and logical shift are `<<<` and `>>>`
ut this language uses `<<` and `>>` for logical shift and uses `<<<` and `>>>` for arithmetic shift.
in bit operation, logical shift is usually used and sometimes arithmetic shift is.
so logical shift operator is shorter in this language.

### 5.2 Unary Operator

```py
# - minus sign
-1
# logical/bit not
!1 # this is bit not
!true # this is logical not
```

increment and decrement are not supported

### 5.3 If and Match

in this language if and match are expression

```py
# if expression
if true: # this evaluated as void
    config.warning("warning")

value := if true: # this evaluated as int
   0 # if matching this
else:
   1 # and this type

value = if true:
    0
elif false:
    1 # else if is supported by elif
else:
    2


# match expression
value = match 1:
    1 => 2 # each statement should have
    3 => value
    _ => 0
```

because of current parser implementation limitation,
code like below may cause an error

```py
p :=
    if value == 1:
        32
    else:
        64
```

```
src2json: error: expect token ident but found keyword
/usr/brgen/sample.bgn:4:5:
   4|    else:
         ^^^^
```

### 5.3 Assignment Statement

assignment makes temporary identifier
3 types of assignment are defined

```py
# := define variable
# define variable identifier
x := 10
# = assign
# rewrite variable
x = 2
# ::= define constant
# define constant value
X ::= 30
```

assignment to constant are invalid.

define constant should be implemented even **if global scope**

### 5.4 Other

```py
# () brackets
(1 + 2)* 3
# a.b member access
a.b #this syntax is allowed if a is enum name, imported name, or format instance
# call(arg1,arg2) call
a(b) # in this language function call is specially treated explained below
# arr_or_map[1] index
a[1] # user only can define array
```

if GID, builtin function can return map like object

### 5.4.1 Function

```py
fn f(param :u32) -> u32:
    return 0
```

## 6. Format

format is a unit of data structure that is converted to struct and encoder/decoder/logger/validator etc.. functions

### 6.1. As-is Format

As-is format is represent encoding/decoding format as is
field is interpreted in order

```py
format ExampleAsIsFormat:
    # fundamental types
    # integer number
    # support for integer number
    # belows are endian unspecified (endian will decided by environment)
    # uN -> N bit unsigned integer
    # sN -> N bit signed integer
    # belows are endian specified
    # ubN -> N bit big endian unsigned integer
    # usN -> N bit big endian signed integer
    # ulN -> N bit little endian unsigned integer
    # slN -> N bit little endian signed integer
    # N is larger than 0 (not 0 and smaller)
    # maximum limits and valid ranges are GID
    # but MUST support u8,s8,u16,s16,u32,s32,u64,s64
    sample1 :u8
    sample2 :u16
    sample3 :u32
    sample4 :u64
    sample5 :s8
    sample6 :s16
    sample7 :s32
    sample8 :s64

    # support for floating point number are GID
    # representation are fN and


    # less than 8 bit also allowed if generator allowed
    # representation in program (packed in one byte? or using bit field?),
    # and byte alignment requirement (allow non-byte aligned format or disallow) are GID
    prefix :u1
    data1 :u7

    # conditional format support are GID but implementing is recommended
    # for example, when decoding format, read prefix prior then judge and read data2 as u8 if prefix == 1 else read data2 as u16
    #              when encoding format, write data2 as u8 if prefix == 1 else write data2 as u16
    #              when logging format, if text format, write as text, else if binary format then write like encoding format
    # detail of how to hold data(union in C?std::variant in C++?enum in Rust?interface in Go?interface and derivative in C#/Java?) is GID
    if prefix == 1:
        data2 :u8
    else:
        data2 :u16

    # this is also conditional format that is GID
    match prefix:
      1 => data3 :u8
      0 => data3 :u16

    # array types are supported if GID
    # generator implementation should support 2 type array
    # first is fixed length array that inner brackets value are constant
    # second is variable length array(VLA) decided by other field
    # VLA length decision field MUST be prior to VLA field
    # to explain define:
    # len   :u8
    # value :[len]type
    # if generated format is symmetric (len is only represented by field to define length in this format; such as C's set of char* and int)
    # encoding and decoding are simply defined (encode/decode as is)
    # if generated format is asymmetric (representation of value also holds length field; such as C++'s std::vector)
    # generator should use len to decide value's length when decoding and should use length of value if encoding
    # if value length is like `value :[len+1]type` use it directly when decoding
    # and when encoding, len should be calculated from `len = value.length - 1` with solving linear equation
    # when value length is like `value :[len1+len2]`, decoding is easy but decision of `len1` and `len2` are too difficult to solve when encoding
    # generator implementer can provide other mechanism to decide len1 or len2, or make it an error; that is GID
    fixed :[16]u8
    len :u8
    data3 :[len]u8

    # boolean expression that is unused for any other purpose is
    # represented as assertion
    # assertion failure makes error or warning.
    # error or waring selection are GID
    sample1 == 2 || sample2 > 2

    # yes we can define format inner format
    format Embed:
        data :u8

    # this is embed representation of type
    # how to represent this (expand in the class?using class inner class?or using embed representation in Go?) is GID
    :Embed


```

### 6.2 Custom Format

Custom format is defined using encode() and decode() function
fields are only a reference of how struct holds value
in decode function, fields are variables.
in encode function, fields are constant

actually below's As-Is format is almost same as this ExampleCustomFormat format
so use below instead of this if you want to use like this

```py
format ExampleCustomFormat:
    prefix :u1
    if prefix == 1:
        field :u63
    else:
        field :u31
```

```py
format ExampleCustomFormat:

    fn encode():
        top = input.peek.u8()
        if top&0x80 != 0:
            field = input.u64()
            field = field & config.u63.max
        else:
            field = input.u32()
            field = field & config.u63.max

    fn decode():
        if field < env.u31.max:
            output.u32(field)
        elif field < env.u63.max:
            output.u64(field)
        else:
            env.error("too large number")
```

## 7. Enum

enum are enumeration of numbers or strings or one type selection

```py
enum ExampleEnum:
    :u8  # this prefer base type definition
    A = 0 # here define A and it's number as 0
    B
    C    # B and C are interpreted as 1 and 3 like C enum or Go iota
    D  = 0xFF # yes this is 0xFF
    E = "hello" # here string are also allowed (but in this situation,maybe error)
    F         # E is also be "hello"
    G = "foo"
```

## 8. Special Literals

### 8.1. Input

```py
input.peek # this is peek proxy object. peek should implement same methods and properties of input
input.uN() # read N bit and parse into uN.  N is greater than 0 value
```

TODO(on-keyday): write more

### 8.2 Output

```py
output.uN(x) # write x as N bit integer into . if type of x is larger than N bit, generator implementor must insert size check
```

TODO(on-keyday): write more

### 8.3 Environment

belows 4 method are for custom error handling and should be implemented

```py
config.error(message) # report error and abort operation
config.warning(message) # report warning not abort
config.assert(c,message) # report error and abort operation if c is false
config.expect(c,message) # report warning not abort if c is false
```

below properties are useful for parsing

```py
config.uN # property set for N bit integer
config.uN.max # maximum value of N bit integer
config.uN.min # minimum value of N bit integer
```

below are handled by AST parser

```py
config.import("file name") # import file and replace ast
```

below are marker for generator

```py
config.export(Ident1,Ident2) # make Ident instantiate in public (public means accessible from other code than generated code)
# in default, all format are public
# if use config.export, then other format are private
# this is used when non-byte aligned format are there and
config.internal(Ident1,Ident2) # make Ident instantiate in private (private means not accessible from other code than generated code)

# both config.export and config.internal cannot be used in same file
# you should choose which one to use
```

TODO(on-keyday): write more

## 9. Builtin

generator implementer MUST provide allowed name (builtin) list of input,output,and env methods and property
and global name definition

below are implemented by parser that is resolved at meta level

```py
available(ident) # boolean value. Ident must be field value. If Ident is union type, then this is converted to condition that ident appear or not, otherwise always evaluated as true.
```

below are future implement plan, but not implemented yet

```py
exists(name) # name is exists in current scope
is_builtin(name) # name is exists as builtin symbol
has_method(name,method) # name has method
has_property(name,property) # name has property
```

## 10. Error Handling

in this language, error handling about IO should be implicit
that is generator should insert appropriate error handling method for each field/Format, encoding/decoding
when encoding, validation should appear in first step of encoding
when decoding, validation should appear in each step of decoding

```py
error("string"[,"type"]) # error string and type to be converted
```

### 10.1 Assertion

floating boolean expression which contains:

- ==
- !=
- &lt;=
- &gt;=
- &lt;
- &gt;
- ||
- &&

is interpreted as assertion
assertion with error should be treated with same error handling mechanism of normal encode/decode error

## 11. Security Considerations

security of this product is almost depends on generator implementation.
most common problem in this field are Buffer Overflow Prevention (BOP).
some of network protocols depends on modulo of field max bits
generator should follow below guidelines

1. secure is more valuable than fast
   even if generated code are very fast like thunderbolt, no secure product has no meaning.
   TODO(on-keyday): write more

## Appendix A. AST representation

representation of ast in json

```
node_type -> node type
optional<T> -> optional value of T
shared_ptr<T> -> strong reference to T
weak_ptr<T> -> weak reference to T
array<T> -> array of T
one_of -> classes which can be node_type
base_node_type -> base class of node
loc - location in source code

```

```json
{
  "node": [
    {
      "node_type": "node",
      "one_of": [
        "program",
        "comment",
        "comment_group",
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
        "cast",
        "available",
        "stmt",
        "loop",
        "indent_block",
        "match_branch",
        "union_candidate",
        "return",
        "break",
        "continue",
        "assert",
        "implicit_yield",
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
        "struct_union_type",
        "union_type",
        "range_type",
        "enum_type",
        "literal",
        "int_literal",
        "bool_literal",
        "str_literal",
        "input",
        "output",
        "config",
        "member",
        "field",
        "format",
        "state",
        "enum",
        "enum_member",
        "function",
        "builtin_function"
      ]
    },
    {
      "node_type": "program",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {
        "struct_type": "shared_ptr<struct_type>",
        "elements": "array<shared_ptr<node>>",
        "global_scope": "shared_ptr<scope>"
      }
    },
    {
      "node_type": "comment",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {
        "comment": "string"
      }
    },
    {
      "node_type": "comment_group",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {
        "comments": "array<shared_ptr<comment>>"
      }
    },
    {
      "node_type": "expr",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level"
      },
      "one_of": [
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
        "cast",
        "available",
        "literal",
        "int_literal",
        "bool_literal",
        "str_literal",
        "input",
        "output",
        "config"
      ]
    },
    {
      "node_type": "binary",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "op": "binary_op",
        "left": "shared_ptr<expr>",
        "right": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "unary",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "op": "unary_op",
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "cond",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "cond": "shared_ptr<expr>",
        "then": "shared_ptr<expr>",
        "els_loc": "loc",
        "els": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "ident",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "ident": "string",
        "usage": "ident_usage",
        "base": "weak_ptr<node>",
        "scope": "shared_ptr<scope>"
      }
    },
    {
      "node_type": "call",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "callee": "shared_ptr<expr>",
        "raw_arguments": "shared_ptr<expr>",
        "arguments": "array<shared_ptr<expr>>",
        "end_loc": "loc"
      }
    },
    {
      "node_type": "if",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "cond_scope": "shared_ptr<scope>",
        "cond": "shared_ptr<expr>",
        "then": "shared_ptr<indent_block>",
        "els": "shared_ptr<node>"
      }
    },
    {
      "node_type": "member_access",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "target": "shared_ptr<expr>",
        "member": "shared_ptr<ident>",
        "base": "weak_ptr<node>"
      }
    },
    {
      "node_type": "paren",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "expr": "shared_ptr<expr>",
        "end_loc": "loc"
      }
    },
    {
      "node_type": "index",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "expr": "shared_ptr<expr>",
        "index": "shared_ptr<expr>",
        "end_loc": "loc"
      }
    },
    {
      "node_type": "match",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "cond_scope": "shared_ptr<scope>",
        "cond": "shared_ptr<expr>",
        "branch": "array<shared_ptr<node>>"
      }
    },
    {
      "node_type": "range",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "op": "binary_op",
        "start": "shared_ptr<expr>",
        "end": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "tmp_var",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "tmp_var": "uint"
      }
    },
    {
      "node_type": "block_expr",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "calls": "array<shared_ptr<node>>",
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "import",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "path": "string",
        "base": "shared_ptr<call>",
        "import_desc": "shared_ptr<program>"
      }
    },
    {
      "node_type": "cast",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "base": "shared_ptr<call>",
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "available",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "base": "shared_ptr<call>",
        "target": "shared_ptr<ident>"
      }
    },
    {
      "node_type": "stmt",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {},
      "one_of": [
        "loop",
        "indent_block",
        "match_branch",
        "union_candidate",
        "return",
        "break",
        "continue",
        "assert",
        "implicit_yield",
        "member",
        "field",
        "format",
        "state",
        "enum",
        "enum_member",
        "function",
        "builtin_function"
      ]
    },
    {
      "node_type": "loop",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "cond_scope": "shared_ptr<scope>",
        "init": "shared_ptr<expr>",
        "cond": "shared_ptr<expr>",
        "step": "shared_ptr<expr>",
        "body": "shared_ptr<indent_block>"
      }
    },
    {
      "node_type": "indent_block",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "struct_type": "shared_ptr<struct_type>",
        "elements": "array<shared_ptr<node>>",
        "scope": "shared_ptr<scope>"
      }
    },
    {
      "node_type": "match_branch",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "cond": "shared_ptr<expr>",
        "sym_loc": "loc",
        "then": "shared_ptr<node>"
      }
    },
    {
      "node_type": "union_candidate",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "cond": "weak_ptr<expr>",
        "field": "weak_ptr<field>"
      }
    },
    {
      "node_type": "return",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "break",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {}
    },
    {
      "node_type": "continue",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {}
    },
    {
      "node_type": "assert",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "cond": "shared_ptr<binary>"
      }
    },
    {
      "node_type": "implicit_yield",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "type",
      "base_node_type": ["node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint"
      },
      "one_of": [
        "int_type",
        "ident_type",
        "int_literal_type",
        "str_literal_type",
        "void_type",
        "bool_type",
        "array_type",
        "function_type",
        "struct_type",
        "struct_union_type",
        "union_type",
        "range_type",
        "enum_type"
      ]
    },
    {
      "node_type": "int_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "endian": "endian",
        "is_signed": "bool",
        "is_common_supported": "bool"
      }
    },
    {
      "node_type": "ident_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "ident": "shared_ptr<ident>",
        "base": "weak_ptr<type>"
      }
    },
    {
      "node_type": "int_literal_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "base": "weak_ptr<int_literal>"
      }
    },
    {
      "node_type": "str_literal_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "base": "weak_ptr<str_literal>"
      }
    },
    {
      "node_type": "void_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint"
      }
    },
    {
      "node_type": "bool_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint"
      }
    },
    {
      "node_type": "array_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "end_loc": "loc",
        "base_type": "shared_ptr<type>",
        "length": "shared_ptr<expr>",
        "length_value": "uint",
        "has_const_length": "bool"
      }
    },
    {
      "node_type": "function_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "return_type": "shared_ptr<type>",
        "parameters": "array<shared_ptr<type>>"
      }
    },
    {
      "node_type": "struct_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "fields": "array<shared_ptr<member>>",
        "base": "weak_ptr<node>",
        "recursive": "bool"
      }
    },
    {
      "node_type": "struct_union_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "structs": "array<shared_ptr<struct_type>>",
        "base": "weak_ptr<expr>",
        "union_fields": "array<weak_ptr<field>>"
      }
    },
    {
      "node_type": "union_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "cond": "weak_ptr<expr>",
        "candidates": "array<shared_ptr<union_candidate>>",
        "base_type": "weak_ptr<struct_union_type>",
        "common_type": "shared_ptr<type>"
      }
    },
    {
      "node_type": "range_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "base_type": "shared_ptr<type>",
        "range": "weak_ptr<range>"
      }
    },
    {
      "node_type": "enum_type",
      "base_node_type": ["type", "node"],
      "loc": "loc",
      "body": {
        "is_explicit": "bool",
        "is_int_set": "bool",
        "bit_alignment": "bit_alignment",
        "bit_size": "uint",
        "base": "weak_ptr<enum>"
      }
    },
    {
      "node_type": "literal",
      "base_node_type": ["expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level"
      },
      "one_of": [
        "int_literal",
        "bool_literal",
        "str_literal",
        "input",
        "output",
        "config"
      ]
    },
    {
      "node_type": "int_literal",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "value": "string"
      }
    },
    {
      "node_type": "bool_literal",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "value": "bool"
      }
    },
    {
      "node_type": "str_literal",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level",
        "value": "string"
      }
    },
    {
      "node_type": "input",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level"
      }
    },
    {
      "node_type": "output",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level"
      }
    },
    {
      "node_type": "config",
      "base_node_type": ["literal", "expr", "node"],
      "loc": "loc",
      "body": {
        "expr_type": "shared_ptr<type>",
        "constant_level": "constant_level"
      }
    },
    {
      "node_type": "member",
      "base_node_type": ["stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>"
      },
      "one_of": [
        "field",
        "format",
        "state",
        "enum",
        "enum_member",
        "function",
        "builtin_function"
      ]
    },
    {
      "node_type": "field",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "colon_loc": "loc",
        "field_type": "shared_ptr<type>",
        "raw_arguments": "shared_ptr<expr>",
        "arguments": "array<shared_ptr<expr>>",
        "bit_alignment": "bit_alignment",
        "follow": "follow"
      }
    },
    {
      "node_type": "format",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "body": "shared_ptr<indent_block>"
      }
    },
    {
      "node_type": "state",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "body": "shared_ptr<indent_block>"
      }
    },
    {
      "node_type": "enum",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "scope": "shared_ptr<scope>",
        "colon_loc": "loc",
        "base_type": "shared_ptr<type>",
        "members": "array<shared_ptr<enum_member>>",
        "enum_type": "shared_ptr<enum_type>"
      }
    },
    {
      "node_type": "enum_member",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "expr": "shared_ptr<expr>"
      }
    },
    {
      "node_type": "function",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "parameters": "array<shared_ptr<field>>",
        "return_type": "shared_ptr<type>",
        "body": "shared_ptr<indent_block>",
        "func_type": "shared_ptr<function_type>",
        "is_cast": "bool",
        "cast_loc": "loc"
      }
    },
    {
      "node_type": "builtin_function",
      "base_node_type": ["member", "stmt", "node"],
      "loc": "loc",
      "body": {
        "belong": "weak_ptr<member>",
        "belong_struct": "weak_ptr<struct_type>",
        "ident": "shared_ptr<ident>",
        "func_type": "shared_ptr<function_type>"
      }
    }
  ],
  "struct": {
    "scope": {
      "prev": "weak_ptr<scope>",
      "next": "shared_ptr<scope>",
      "branch": "shared_ptr<scope>",
      "ident": "array<std::weak_ptr<ident>>",
      "owner": "weak_ptr<node>",
      "branch_root": "bool"
    },
    "pos": {
      "begin": "uint",
      "end": "uint"
    },
    "loc": {
      "pos": "pos",
      "file": "uint",
      "line": "uint",
      "col": "uint"
    },
    "token": {
      "tag": "token_tag",
      "token": "string",
      "loc": "loc"
    },
    "raw_scope": {
      "prev": "optional<uintptr>",
      "next": "optional<uintptr>",
      "branch": "optional<uintptr>",
      "ident": "array<uintptr>",
      "owner": "optional<uintptr>",
      "branch_root": "bool"
    },
    "raw_node": {
      "node_type": "node_type",
      "loc": "loc",
      "body": "any"
    },
    "src_error_entry": {
      "msg": "string",
      "file": "string",
      "loc": "loc",
      "src": "string",
      "warn": "bool"
    },
    "src_error": {
      "errs": "array<src_error_entry>"
    },
    "json_ast": {
      "node": "array<raw_node>",
      "scope": "array<raw_scope>"
    },
    "ast_file": {
      "files": "array<string>",
      "ast": "optional<json_ast>",
      "error": "optional<src_error>"
    },
    "token_file": {
      "files": "array<string>",
      "tokens": "optional<array<token>>",
      "error": "optional<src_error>"
    }
  },
  "enum": {
    "node_type": [
      {
        "name": "program",
        "value": "program"
      },
      {
        "name": "comment",
        "value": "comment"
      },
      {
        "name": "comment_group",
        "value": "comment_group"
      },
      {
        "name": "expr",
        "value": "expr"
      },
      {
        "name": "binary",
        "value": "binary"
      },
      {
        "name": "unary",
        "value": "unary"
      },
      {
        "name": "cond",
        "value": "cond"
      },
      {
        "name": "ident",
        "value": "ident"
      },
      {
        "name": "call",
        "value": "call"
      },
      {
        "name": "if",
        "value": "if"
      },
      {
        "name": "member_access",
        "value": "member_access"
      },
      {
        "name": "paren",
        "value": "paren"
      },
      {
        "name": "index",
        "value": "index"
      },
      {
        "name": "match",
        "value": "match"
      },
      {
        "name": "range",
        "value": "range"
      },
      {
        "name": "tmp_var",
        "value": "tmp_var"
      },
      {
        "name": "block_expr",
        "value": "block_expr"
      },
      {
        "name": "import",
        "value": "import"
      },
      {
        "name": "cast",
        "value": "cast"
      },
      {
        "name": "available",
        "value": "available"
      },
      {
        "name": "stmt",
        "value": "stmt"
      },
      {
        "name": "loop",
        "value": "loop"
      },
      {
        "name": "indent_block",
        "value": "indent_block"
      },
      {
        "name": "match_branch",
        "value": "match_branch"
      },
      {
        "name": "union_candidate",
        "value": "union_candidate"
      },
      {
        "name": "return",
        "value": "return"
      },
      {
        "name": "break",
        "value": "break"
      },
      {
        "name": "continue",
        "value": "continue"
      },
      {
        "name": "assert",
        "value": "assert"
      },
      {
        "name": "implicit_yield",
        "value": "implicit_yield"
      },
      {
        "name": "type",
        "value": "type"
      },
      {
        "name": "int_type",
        "value": "int_type"
      },
      {
        "name": "ident_type",
        "value": "ident_type"
      },
      {
        "name": "int_literal_type",
        "value": "int_literal_type"
      },
      {
        "name": "str_literal_type",
        "value": "str_literal_type"
      },
      {
        "name": "void_type",
        "value": "void_type"
      },
      {
        "name": "bool_type",
        "value": "bool_type"
      },
      {
        "name": "array_type",
        "value": "array_type"
      },
      {
        "name": "function_type",
        "value": "function_type"
      },
      {
        "name": "struct_type",
        "value": "struct_type"
      },
      {
        "name": "struct_union_type",
        "value": "struct_union_type"
      },
      {
        "name": "union_type",
        "value": "union_type"
      },
      {
        "name": "range_type",
        "value": "range_type"
      },
      {
        "name": "enum_type",
        "value": "enum_type"
      },
      {
        "name": "literal",
        "value": "literal"
      },
      {
        "name": "int_literal",
        "value": "int_literal"
      },
      {
        "name": "bool_literal",
        "value": "bool_literal"
      },
      {
        "name": "str_literal",
        "value": "str_literal"
      },
      {
        "name": "input",
        "value": "input"
      },
      {
        "name": "output",
        "value": "output"
      },
      {
        "name": "config",
        "value": "config"
      },
      {
        "name": "member",
        "value": "member"
      },
      {
        "name": "field",
        "value": "field"
      },
      {
        "name": "format",
        "value": "format"
      },
      {
        "name": "state",
        "value": "state"
      },
      {
        "name": "enum",
        "value": "enum"
      },
      {
        "name": "enum_member",
        "value": "enum_member"
      },
      {
        "name": "function",
        "value": "function"
      },
      {
        "name": "builtin_function",
        "value": "builtin_function"
      }
    ],
    "unary_op": [
      {
        "name": "not",
        "value": "!"
      },
      {
        "name": "minus_sign",
        "value": "-"
      }
    ],
    "binary_op": [
      {
        "name": "mul",
        "value": "*"
      },
      {
        "name": "div",
        "value": "/"
      },
      {
        "name": "mod",
        "value": "%"
      },
      {
        "name": "left_arithmetic_shift",
        "value": "<<<"
      },
      {
        "name": "right_arithmetic_shift",
        "value": ">>>"
      },
      {
        "name": "left_logical_shift",
        "value": "<<"
      },
      {
        "name": "right_logical_shift",
        "value": ">>"
      },
      {
        "name": "bit_and",
        "value": "&"
      },
      {
        "name": "add",
        "value": "+"
      },
      {
        "name": "sub",
        "value": "-"
      },
      {
        "name": "bit_or",
        "value": "|"
      },
      {
        "name": "bit_xor",
        "value": "^"
      },
      {
        "name": "equal",
        "value": "=="
      },
      {
        "name": "not_equal",
        "value": "!="
      },
      {
        "name": "less",
        "value": "<"
      },
      {
        "name": "less_or_eq",
        "value": "<="
      },
      {
        "name": "grater",
        "value": ">"
      },
      {
        "name": "grater_or_eq",
        "value": ">="
      },
      {
        "name": "logical_and",
        "value": "&&"
      },
      {
        "name": "logical_or",
        "value": "||"
      },
      {
        "name": "cond_op1",
        "value": "?"
      },
      {
        "name": "cond_op2",
        "value": ":"
      },
      {
        "name": "range_exclusive",
        "value": ".."
      },
      {
        "name": "range_inclusive",
        "value": "..="
      },
      {
        "name": "assign",
        "value": "="
      },
      {
        "name": "define_assign",
        "value": ":="
      },
      {
        "name": "const_assign",
        "value": "::="
      },
      {
        "name": "add_assign",
        "value": "+="
      },
      {
        "name": "sub_assign",
        "value": "-="
      },
      {
        "name": "mul_assign",
        "value": "*="
      },
      {
        "name": "div_assign",
        "value": "/="
      },
      {
        "name": "mod_assign",
        "value": "%="
      },
      {
        "name": "left_shift_assign",
        "value": "<<="
      },
      {
        "name": "right_shift_assign",
        "value": ">>="
      },
      {
        "name": "bit_and_assign",
        "value": "&="
      },
      {
        "name": "bit_or_assign",
        "value": "|="
      },
      {
        "name": "bit_xor_assign",
        "value": "^="
      },
      {
        "name": "comma",
        "value": ","
      }
    ],
    "ident_usage": [
      {
        "name": "unknown",
        "value": "unknown"
      },
      {
        "name": "reference",
        "value": "reference"
      },
      {
        "name": "define_variable",
        "value": "define_variable"
      },
      {
        "name": "define_const",
        "value": "define_const"
      },
      {
        "name": "define_field",
        "value": "define_field"
      },
      {
        "name": "define_format",
        "value": "define_format"
      },
      {
        "name": "define_state",
        "value": "define_state"
      },
      {
        "name": "define_enum",
        "value": "define_enum"
      },
      {
        "name": "define_enum_member",
        "value": "define_enum_member"
      },
      {
        "name": "define_fn",
        "value": "define_fn"
      },
      {
        "name": "define_cast_fn",
        "value": "define_cast_fn"
      },
      {
        "name": "define_arg",
        "value": "define_arg"
      },
      {
        "name": "reference_type",
        "value": "reference_type"
      },
      {
        "name": "reference_member",
        "value": "reference_member"
      },
      {
        "name": "maybe_type",
        "value": "maybe_type"
      },
      {
        "name": "reference_builtin_fn",
        "value": "reference_builtin_fn"
      }
    ],
    "endian": [
      {
        "name": "unspec",
        "value": "unspec"
      },
      {
        "name": "big",
        "value": "big"
      },
      {
        "name": "little",
        "value": "little"
      }
    ],
    "token_tag": [
      {
        "name": "indent",
        "value": "indent"
      },
      {
        "name": "space",
        "value": "space"
      },
      {
        "name": "line",
        "value": "line"
      },
      {
        "name": "punct",
        "value": "punct"
      },
      {
        "name": "int_literal",
        "value": "int_literal"
      },
      {
        "name": "bool_literal",
        "value": "bool_literal"
      },
      {
        "name": "str_literal",
        "value": "str_literal"
      },
      {
        "name": "keyword",
        "value": "keyword"
      },
      {
        "name": "ident",
        "value": "ident"
      },
      {
        "name": "comment",
        "value": "comment"
      },
      {
        "name": "error",
        "value": "error"
      },
      {
        "name": "unknown",
        "value": "unknown"
      }
    ],
    "constant_level": [
      {
        "name": "unknown",
        "value": "unknown"
      },
      {
        "name": "constant",
        "value": "constant"
      },
      {
        "name": "const_variable",
        "value": "const_variable"
      },
      {
        "name": "variable",
        "value": "variable"
      }
    ],
    "bit_alignment": [
      {
        "name": "byte_aligned",
        "value": "byte_aligned"
      },
      {
        "name": "bit_1",
        "value": "bit_1"
      },
      {
        "name": "bit_2",
        "value": "bit_2"
      },
      {
        "name": "bit_3",
        "value": "bit_3"
      },
      {
        "name": "bit_4",
        "value": "bit_4"
      },
      {
        "name": "bit_5",
        "value": "bit_5"
      },
      {
        "name": "bit_6",
        "value": "bit_6"
      },
      {
        "name": "bit_7",
        "value": "bit_7"
      },
      {
        "name": "not_target",
        "value": "not_target"
      },
      {
        "name": "not_decidable",
        "value": "not_decidable"
      }
    ],
    "follow": [
      {
        "name": "unknown",
        "value": "unknown"
      },
      {
        "name": "end",
        "value": "end"
      },
      {
        "name": "fixed",
        "value": "fixed"
      },
      {
        "name": "constant",
        "value": "constant"
      },
      {
        "name": "normal",
        "value": "normal"
      }
    ]
  }
}
```

## Appendix B. Operator Precedence and Associative Order

```
high
postfix        () []
unary          - !
multiplicative *  /  %  <<  >>  &  left to right
additive       +  -  |  ^          left to right
compare        == != < > <= >=     unbounded (appear in same level is an error)
logical and    &&                  left to right
logical or     ||                  left to right
range          .. ..=              unbounded
low
```

## C. Generator Implementer Guide

this is guide for code generator implementation

1. first step: AST traversal
   first step to write generator is AST traversal.
   AST traversal means walking each node.
   node types are too many
   but you should not remember all of node.
   you first remember Node, Format, Field and IntType
   Node is base of all node type.
   Format is a unit of encode/decode.
   Field is a minimum unit to encode/decode
   IntType represents integer type

## History:

- 2023/09/08: write first version draft version: 0.0.0.d1
- 2023/09/14: rewrite this as markdown text version: 0.0.0.d1.a
  - change keyword env -> config
  - change comment style // and /\*\*/ to #
  - add range(.. and ..=) expression
  - remove needless comment
- 2023/09/23: write draft version 2: 0.0.0.d2
- 2023/09/24: add <<< and >>>. add note for Java user
- 2023/09/26: add ast representation json: 0.0.0.d2.b
- 2023/09/25: add code limitation note of `if`
- 2023/10/04: fix env to config: 0.0.0.d2.c
- 2023/10/04: add config.import and config.export
- 2023/12/05: update version: 0.0.0.d3
  - update json types
  - explicit error
  - available builtin
  - first step of generator
