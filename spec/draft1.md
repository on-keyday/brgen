
# brgen language specialization
#####
##### draft version 1
##### lang-version: 0.0.0.d1.a # draft 1
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
 1. if in local scope, search variable defined before in same scope, 
 1.a if found, link variable with definition location
 1.b if not found then up the scope then start lookup from 1. 
 2. if in global scope, then look up from first of definition to end 

### 3.2 Type lookup rule
 1. look up from first of definition to end in scope (no difference between local and global)
 2. if not found, then up the 

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
above are treated as 
format
enum
fn
if
elif
else
match
loop
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
# >> bit right shift
2 >> 1
# << bit left shift
1 << 2
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
```

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
    env.warning("warning")

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
TODO(on-keyday): write about treatment of function and function arguments

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
    A :0 # here define A and it's number as 0
    B
    C    # B and C are interpreted as 1 and 3 like C enum or Go iota
    D :0xFF # yes this is 0xFF
    E:"hello" # here string are also allowed (but in this situation,maybe error)
    F         # E is also be "hello"
    G :"foo" 
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
env.error(message) # report error and abort operation
env.warning(message) # report warning not abort
env.assert(c,message) # report error and abort operation if c is false
env.expect(c,message) # report warning not abort if c is false
```
below properties are useful for parsing
```py
env.uN # property set for N bit integer
env.uN.max # maximum value of N bit integer
env.uN.min # minimum value of N bit integer 
```
TODO(on-keyday): write more

## 9. Builtin
generator implementer MUST provide allowed name (builtin) list of input,output,and env methods and property 
and global name definition 

below are implemented by parser that is resolved at meta level 
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
### 10.1 Assertion
floating boolean expression is interpreted as assertion
assertion with error should be treated with same error handling mechanism of normal encode/decode error


## 11. Security Considerations
security of this product is almost depends on generator implementation.
most common problem in this field are Buffer Overflow Prevention (BOP)
generator should follow below guidelines
  1. secure is more valuable than fast
   even if generated code are very fast like thunderbolt, no secure product has no meaning.
 TODO(on-keyday): write more

## Appendix A. AST representation
representation of ast
```
    every node contains {
        node_type: AST node type
        loc: location in source code and source file
            pos: position in source code
              begin: start position of source code
              end: end position of source code
        file: file index
        comment: comment value related with this node
    }
```

## Appendix B. Operator Precedence and Associative Order
```
high
postfix        () []
(here ident and literal, inner brackets expression)
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
TODO(on-keyday): write this

## History:
+ 2023/09/08: write first version draft version: 0.0.0.d1
+ 2023/09/14: rewrite this as markdown text version: 0.0.0.d1.a
