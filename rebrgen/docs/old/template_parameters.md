# Template Document
This document describes the variables that can be used in the code generator-generator hooks.
Code generator-generator hooks are pieces of C++ code that are inserted into the generator code.
## Words
### reference
A reference to a several object in the binary module.
They are represented as rebgn::Varint. they are not supposed to be used directly.
### type reference
A reference to a type in the binary module.
They are represented as rebgn::StorageRef. they are not supposed to be used directly.
### identifier
An identifier of a several object (e.g. function, variable, types, etc.)
They are represented as std::string. use them for generating code.
### EvalResult
result of eval() function. it contains the result of the expression evaluation.
### placeholder
in some case, code generator placeholder in `config.json` can be replaced with context specific value by envrionment variable like format.
for example, config name `int_type` can take two env map `BIT_SIZE` and `ALIGNED_BIT_SIZE`.
and use like below:
```json
{
    "int_type": "std::int${ALIGNED_BIT_SIZE}_t"
}
```
and if you set `ALIGNED_BIT_SIZE` to 32, then `int_type` will be `std::int32_t`.
specially, `$DOLLAR` or `${DOLLAR}` is reserved for `$` character.
## function `eval`
### ACCESS
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
##### right_ident_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
##### right_ident
Type: string
Initial Value: ctx.ident(right_ident_ref)
Description: identifier of right operand
##### is_enum_member
Type: bool
Initial Value: ctx.ref(left_eval_ref).op == rebgn::AbstractOp::IMMEDIATE_TYPE
Description: is enum member
#### Placeholder Mappings: 
##### access_style
Name: BASE
Mapped Value: `",left_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:232
Function Name: operator()
Name: IDENT
Mapped Value: `",right_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:232
Function Name: operator()
##### enum_access_style
Name: BASE
Mapped Value: `",left_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:233
Function Name: operator()
Name: IDENT
Mapped Value: `",right_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:233
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",left_eval.result,"::",right_ident,"")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",left_eval.result,".",right_ident,"")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### ADDRESS_OF
#### Variables: 
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
#### Placeholder Mappings: 
##### address_of_placeholder
Name: VALUE
Mapped Value: `",target.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:114
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("&",target.result,"")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### ARRAY_SIZE
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of array
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: array
#### Placeholder Mappings: 
##### size_method
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:265
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",vector_eval.result,".size()")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### ASSIGN
#### Variables: 
##### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assign target
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assign source
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of previous assignment or phi or definition
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(left_ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### BEGIN_COND_BLOCK
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### BINARY
#### Variables: 
##### op
Type: rebgn::BinaryOp
Initial Value: code.bop().value()
Description: binary operator
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of left operand
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: left operand
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of right operand
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: right operand
##### opstr
Type: string
Initial Value: to_string(op)
Description: binary operator string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("({} {} {})", left_eval.result, opstr, right_eval.result)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### CALL
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented CALL*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### CALL_CAST
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented CALL_CAST*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### CAN_READ
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented CAN_READ*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### CAST
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of cast target type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: cast target type
##### from_type_ref
Type: Varint
Initial Value: code.from_type().value()
Description: reference of cast source type
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of cast source value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: cast source value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("({}){}", type, evaluated.result)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### func_style_cast
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:298
Function Name: operator()
### DECLARE_VARIABLE
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_BIT_FIELD
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `field_accessor(code,ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_CONSTANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of constant name
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of constant name
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_FIELD
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `field_accessor(code,ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### self_param
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:149
Function Name: write_parameter_func
### DEFINE_PROPERTY
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `field_accessor(code,ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_VARIABLE_REF
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### EMPTY_OPTIONAL
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"std::nullopt"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### empty_optional
Flag Value: `std::nullopt`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:195
Function Name: operator()
### EMPTY_PTR
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"nullptr"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### empty_pointer
Flag Value: `nullptr`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:190
Function Name: operator()
### FIELD_AVAILABLE
#### Variables: 
##### left_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of field (maybe null)
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_ref), ctx)
Description: field
##### right_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of condition
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(right_ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(right_ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IMMEDIATE_CHAR
#### Variables: 
##### char_code
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate char code
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}", char_code)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IMMEDIATE_FALSE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"false"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### false_literal
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:167
Function Name: operator()
### IMMEDIATE_INT
#### Variables: 
##### value
Type: uint64_t
Initial Value: code.int_value()->value()
Description: immediate value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}", value)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IMMEDIATE_INT64
#### Variables: 
##### value
Type: uint64_t
Initial Value: *code.int_value64()
Description: immediate value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}", value)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IMMEDIATE_STRING
#### Variables: 
##### str
Type: string
Initial Value: ctx.string_table[code.ident().value().value()]
Description: immediate string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("\"{}\"", futils::escape::escape_str<std::string>(str,futils::escape::EscapeFlag::hex,futils::escape::no_escape_set(),futils::escape::escape_all()))`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IMMEDIATE_TRUE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"true"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### true_literal
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:162
Function Name: operator()
### IMMEDIATE_TYPE
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of immediate type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: immediate type
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `type`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### INDEX
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of indexed object
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: indexed object
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of index
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: index
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}[{}]", left_eval.result, right_eval.result)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### INPUT_BIT_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented INPUT_BIT_OFFSET*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### INPUT_BYTE_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### decode_offset
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:135
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",ctx.r(),".offset()")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### IS_LITTLE_ENDIAN
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(fallback), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"std::endian::native == std::endian::little"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### is_little_endian
Flag Value: `std::endian::native == std::endian::little`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:101
Function Name: operator()
### NEW_OBJECT
#### Variables: 
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of object type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: object type
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}()", type)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### OPTIONAL_OF
#### Variables: 
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target object
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target object
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of type of optional (not include optional)
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: type of optional (not include optional)
#### Placeholder Mappings: 
##### optional_of_placeholder
Name: TYPE
Mapped Value: `",type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:126
Function Name: operator()
Name: VALUE
Mapped Value: `",target.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:126
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",target.result,"")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### OUTPUT_BIT_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented OUTPUT_BIT_OFFSET*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### OUTPUT_BYTE_OFFSET
#### Variables: 
#### Placeholder Mappings: 
##### encode_offset
Name: ENCODER
Mapped Value: `",ctx.w(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:144
Function Name: operator()
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `futils::strutil::concat<std::string>("",ctx.w(),".offset()")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### PHI
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of expression
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `eval(ctx.ref(ref), ctx)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable value
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable value
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### REMAIN_BYTES
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `"/*Unimplemented REMAIN_BYTES*/"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### UNARY
#### Variables: 
##### op
Type: rebgn::UnaryOp
Initial Value: code.uop().value()
Description: unary operator
##### target_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of target
##### target
Type: EvalResult
Initial Value: eval(ctx.ref(target_ref), ctx)
Description: target
##### opstr
Type: string
Initial Value: to_string(op)
Description: unary operator string
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("({}{})", opstr, target.result)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
## function `inner_function`
### APPEND
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector (not temporary)
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector (not temporary)
##### new_element_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of new element
##### new_element_eval
Type: EvalResult
Initial Value: eval(ctx.ref(new_element_eval_ref), ctx)
Description: new element
#### Placeholder Mappings: 
##### append_method
Name: ITEM
Mapped Value: `",new_element_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:69
Function Name: operator()
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:69
Function Name: operator()
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:70
Function Name: operator()
### ASSERT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of assertion condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: assertion condition
#### Placeholder Mappings: 
##### assert_method
Name: CONDITION
Mapped Value: `",evaluated.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:244
Function Name: operator()
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:245
Function Name: operator()
### ASSIGN
#### Variables: 
##### left_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of assignment target
##### left_eval
Type: EvalResult
Initial Value: eval(ctx.ref(left_eval_ref), ctx)
Description: assignment target
##### right_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of assignment source
##### right_eval
Type: EvalResult
Initial Value: eval(ctx.ref(right_eval_ref), ctx)
Description: assignment source
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:77
Function Name: operator()
### BACKWARD_INPUT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
#### Placeholder Mappings: 
##### decode_backward
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:88
Function Name: operator()
Name: OFFSET
Mapped Value: `",evaluated.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:88
Function Name: operator()
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:89
Function Name: operator()
### BACKWARD_OUTPUT
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of backward offset to move (in byte)
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: backward offset to move (in byte)
#### Placeholder Mappings: 
##### encode_backward
Name: ENCODER
Mapped Value: `",ctx.w(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:96
Function Name: operator()
Name: OFFSET
Mapped Value: `",evaluated.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:96
Function Name: operator()
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:97
Function Name: operator()
### BEGIN_DECODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### BEGIN_DECODE_SUB_RANGE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
##### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### BEGIN_ENCODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### BEGIN_ENCODE_SUB_RANGE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of sub range length or vector expression
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: sub range length or vector expression
##### sub_range_type
Type: SubRangeType
Initial Value: code.sub_range_type().value()
Description: sub range type (byte_len or replacement)
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### BREAK
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:357
Function Name: operator()
### CALL_DECODE
#### Variables: 
##### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
##### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
##### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
##### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
##### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
##### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
##### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:461
Function Name: operator()
### CALL_ENCODE
#### Variables: 
##### func_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of function
##### func_belong
Type: Varint
Initial Value: ctx.ref(func_ref).belong().value()
Description: reference of function belong
##### func_belong_name
Type: string
Initial Value: type_accessor(ctx.ref(func_belong), ctx)
Description: function belong name
##### func_name
Type: string
Initial Value: ctx.ident(func_ref)
Description: identifier of function
##### obj_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of `this` object
##### obj_eval
Type: EvalResult
Initial Value: eval(ctx.ref(obj_eval_ref), ctx)
Description: `this` object
##### inner_range
Type: string
Initial Value: ctx.range(func_ref)
Description: range of function call range
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:461
Function Name: operator()
### CASE
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:330
Function Name: operator()
##### match_case_separator
Flag Value: `:`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:330
Function Name: operator()
##### match_case_keyword
Flag Value: `case`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:330
Function Name: operator()
### CHECK_UNION
#### Variables: 
##### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
##### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
##### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
##### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
##### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
##### check_type
Type: rebgn::UnionCheckAt
Initial Value: code.check_at().value()
Description: union check location
#### Placeholder Mappings: 
##### check_union_condition
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
##### check_union_fail_return_value
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:428
Function Name: operator()
#### Flag Usage Mappings: 
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:423
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:423
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:422
Function Name: operator()
##### if_keyword
Flag Value: `if`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:422
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:429
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:434
Function Name: operator()
##### empty_pointer
Flag Value: `nullptr`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:434
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:439
Function Name: operator()
##### empty_optional
Flag Value: `std::nullopt`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:439
Function Name: operator()
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:448
Function Name: operator()
### CONTINUE
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:352
Function Name: operator()
### DECLARE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
##### init_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).ref().value()
Description: reference of variable initialization
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
##### type_ref
Type: Varint
Initial Value: ctx.ref(code.ref().value()).type().value()
Description: reference of variable
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### define_variable
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:125
Function Name: operator()
##### omit_type_on_define_var
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:134
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:138
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### define_var_assign
Flag Value: `=`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### var_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### define_var_keyword
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
### DECODE_INT
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECODE_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### decode_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
Name: LEN
Mapped Value: `",size_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
Name: VALUE
Mapped Value: `",vector_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
#### Flag Usage Mappings: 
### DECODE_INT_VECTOR_FIXED
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### decode_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
Name: LEN
Mapped Value: `",size_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
Name: VALUE
Mapped Value: `",vector_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:510
Function Name: operator()
#### Flag Usage Mappings: 
### DECODE_INT_VECTOR_UNTIL_EOF
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
##### decode_bytes_until_eof_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:518
Function Name: operator()
Name: VALUE
Mapped Value: `",evaluated.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:518
Function Name: operator()
#### Flag Usage Mappings: 
### DEFAULT_CASE
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:333
Function Name: operator()
##### match_default_keyword
Flag Value: `default`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:333
Function Name: operator()
### DEFINE_CONSTANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of constant name
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of constant name
##### init_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of constant value
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: constant value
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:184
Function Name: operator()
##### define_const_keyword
Flag Value: `const`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:184
Function Name: operator()
### DEFINE_FUNCTION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of function
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of function
##### func_type
Type: rebgn::FunctionType
Initial Value: code.func_type().value()
Description: function type
##### is_empty_block
Type: bool
Initial Value: i + 1 < ctx.bm.code.size() && ctx.bm.code[i + 1].op == rebgn::AbstractOp::END_FUNCTION
Description: empty block
##### inner_range
Type: Range
Initial Value: range
Description: function range
##### ret_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
##### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### func_keyword
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:10
Function Name: write_func_decl
##### trailing_return_type
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:14
Function Name: write_func_decl
##### func_void_return_type
Flag Value: `void`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:19
Function Name: write_func_decl
##### func_brace_ident_separator
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:23
Function Name: write_func_decl
##### trailing_return_type
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:26
Function Name: write_func_decl
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:285
Function Name: operator()
### DEFINE_VARIABLE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variable
##### init_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of variable initialization
##### init
Type: EvalResult
Initial Value: eval(ctx.ref(init_ref), ctx)
Description: variable initialization
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of variable
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: variable
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### define_variable
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:125
Function Name: operator()
##### omit_type_on_define_var
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:134
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:138
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### define_var_assign
Flag Value: `=`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### var_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
##### define_var_keyword
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:142
Function Name: operator()
### DYNAMIC_ENDIAN
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### ELIF
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
##### is_empty_block
Type: bool
Initial Value: find_next_else_or_end_if(ctx, i, true) == i + 1 || ctx.bm.code[i + 1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK
Description: empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### otbs_on_block_end
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:297
Function Name: operator()
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:301
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:314
Function Name: operator()
##### elif_keyword
Flag Value: `else if`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:314
Function Name: operator()
### ELSE
#### Variables: 
##### is_empty_block
Type: bool
Initial Value: find_next_else_or_end_if(ctx, i, true) == i + 1 || ctx.bm.code[i + 1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK
Description: empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### otbs_on_block_end
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:297
Function Name: operator()
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:301
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:317
Function Name: operator()
##### else_keyword
Flag Value: `else`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:317
Function Name: operator()
### ENCODE_INT
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of element
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: element
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### ENCODE_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### encode_bytes_op
Name: ENCODER
Mapped Value: `",ctx.w(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
Name: LEN
Mapped Value: `",size_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
Name: VALUE
Mapped Value: `",vector_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
#### Flag Usage Mappings: 
### ENCODE_INT_VECTOR_FIXED
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### encode_bytes_op
Name: ENCODER
Mapped Value: `",ctx.w(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
Name: LEN
Mapped Value: `",size_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
Name: VALUE
Mapped Value: `",vector_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:501
Function Name: operator()
#### Flag Usage Mappings: 
### END_CASE
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:347
Function Name: operator()
### END_DECODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### END_ENCODE_PACKED_OPERATION
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### END_FUNCTION
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:347
Function Name: operator()
### END_IF
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:347
Function Name: operator()
### END_LOOP
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:347
Function Name: operator()
### END_MATCH
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:347
Function Name: operator()
### EXHAUSTIVE_MATCH
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:327
Function Name: operator()
##### match_keyword
Flag Value: `switch`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:327
Function Name: operator()
### EXPLICIT_ERROR
#### Variables: 
##### param
Type: Param
Initial Value: code.param().value()
Description: error message parameters
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(param.refs[0]), ctx)
Description: error message
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:252
Function Name: operator()
### IF
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
##### is_empty_block
Type: bool
Initial Value: find_next_else_or_end_if(ctx, i, true) == i + 1 || ctx.bm.code[i + 1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK
Description: empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:311
Function Name: operator()
##### if_keyword
Flag Value: `if`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:311
Function Name: operator()
### INC
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of increment target
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: increment target
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:398
Function Name: operator()
### LENGTH_CHECK
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector to check
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector to check
##### size_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size to check
##### size_eval
Type: EvalResult
Initial Value: eval(ctx.ref(size_eval_ref), ctx)
Description: size to check
#### Placeholder Mappings: 
##### length_check_method
Name: SIZE
Mapped Value: `",size_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:109
Function Name: operator()
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:109
Function Name: operator()
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:110
Function Name: operator()
### LOOP_CONDITION
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
##### is_empty_block
Type: bool
Initial Value: find_next_end_loop(ctx, i) == i + 1
Description: empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:323
Function Name: operator()
##### conditional_loop
Flag Value: `while`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:323
Function Name: operator()
### LOOP_INFINITE
#### Variables: 
##### is_empty_block
Type: bool
Initial Value: find_next_end_loop(ctx, i) == i + 1
Description: empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:320
Function Name: operator()
##### infinity_loop
Flag Value: `for(;;)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:320
Function Name: operator()
### MATCH
#### Variables: 
##### evaluated_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of condition
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: condition
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:306
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:327
Function Name: operator()
##### match_keyword
Flag Value: `switch`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:327
Function Name: operator()
### PEEK_INT_VECTOR
#### Variables: 
##### fallback
Type: Varint
Initial Value: code.fallback().value()
Description: reference of fallback operation
##### inner_range
Type: string
Initial Value: ctx.range(fallback)
Description: range of fallback operation
##### bit_size
Type: uint64_t
Initial Value: code.bit_size()->value()
Description: bit size of element
##### endian
Type: rebgn::Endian
Initial Value: code.endian().value()
Description: endian of element
##### vector_value_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_value
Type: EvalResult
Initial Value: eval(ctx.ref(vector_value_ref), ctx)
Description: vector
##### size_value_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_value
Type: EvalResult
Initial Value: eval(ctx.ref(size_value_ref), ctx)
Description: size
#### Placeholder Mappings: 
##### peek_bytes_op
Name: DECODER
Mapped Value: `",ctx.r(),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:527
Function Name: operator()
Name: LEN
Mapped Value: `",size_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:527
Function Name: operator()
Name: VALUE
Mapped Value: `",vector_value.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:527
Function Name: operator()
#### Flag Usage Mappings: 
### RESERVE_SIZE
#### Variables: 
##### vector_eval_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of vector
##### vector_eval
Type: EvalResult
Initial Value: eval(ctx.ref(vector_eval_ref), ctx)
Description: vector
##### size_eval_ref
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of size
##### size_eval
Type: EvalResult
Initial Value: eval(ctx.ref(size_eval_ref), ctx)
Description: size
##### reserve_type
Type: rebgn::ReserveType
Initial Value: code.reserve_type().value()
Description: reserve vector type
#### Placeholder Mappings: 
##### reserve_size_static
Name: SIZE
Mapped Value: `",size_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:159
Function Name: operator()
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:159
Function Name: operator()
##### reserve_size_dynamic
Name: SIZE
Mapped Value: `",size_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:172
Function Name: operator()
Name: VECTOR
Mapped Value: `",vector_eval.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:172
Function Name: operator()
#### Flag Usage Mappings: 
### RET
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of return value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(ref), ctx)
Description: return value
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:366
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:374
Function Name: operator()
### RET_PROPERTY_SETTER_FAIL
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:392
Function Name: operator()
##### property_setter_fail
Flag Value: `return false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:392
Function Name: operator()
### RET_PROPERTY_SETTER_OK
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:387
Function Name: operator()
##### property_setter_ok
Flag Value: `return true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:387
Function Name: operator()
### RET_SUCCESS
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:382
Function Name: operator()
##### coder_success
Flag Value: `return true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:382
Function Name: operator()
### SWITCH_UNION
#### Variables: 
##### union_member_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of current union member
##### union_ref
Type: Varint
Initial Value: ctx.ref(union_member_ref).belong().value()
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_member_index
Type: uint64_t
Initial Value: ctx.ref(union_member_ref).int_value()->value()
Description: current union member index
##### union_member_ident
Type: string
Initial Value: ctx.ident(union_member_ref)
Description: identifier of union member
##### union_ident
Type: string
Initial Value: ctx.ident(union_ref)
Description: identifier of union
##### union_field_ident
Type: EvalResult
Initial Value: eval(ctx.ref(union_field_ref), ctx)
Description: union field
#### Placeholder Mappings: 
##### check_union_condition
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:421
Function Name: operator()
##### switch_union
Name: FIELD_IDENT
Mapped Value: `",union_field_ident.result,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
Name: MEMBER_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_member_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
Name: MEMBER_IDENT
Mapped Value: `",union_member_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
Name: MEMBER_INDEX
Mapped Value: `",futils::number::to_string<std::string>(union_member_index),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
Name: UNION_FULL_IDENT
Mapped Value: `",type_accessor(ctx.ref(union_ref),ctx),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
Name: UNION_IDENT
Mapped Value: `",union_ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:444
Function Name: operator()
#### Flag Usage Mappings: 
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:423
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:423
Function Name: operator()
##### condition_has_parentheses
Flag Value: `true`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:422
Function Name: operator()
##### if_keyword
Flag Value: `if`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:422
Function Name: operator()
##### end_of_statement
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:445
Function Name: operator()
##### block_end
Flag Value: `}`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_function.cpp:448
Function Name: operator()
## function `inner_block`
### DECLARE_BIT_FIELD
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of bit field
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of bit field
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of bit field
##### type_ref
Type: Varint
Initial Value: ctx.ref(ref).type().value()
Description: reference of bit field type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: bit field type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECLARE_ENUM
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of ENUM
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of ENUM
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of ENUM
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECLARE_FORMAT
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FORMAT
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FORMAT
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of FORMAT
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECLARE_FUNCTION
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of FUNCTION
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of FUNCTION
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of FUNCTION
##### func_type
Type: rebgn::FunctionType
Initial Value: ctx.ref(ref).func_type().value()
Description: function type
##### ret_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
##### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### format_nested_function
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:47
Function Name: operator()
### DECLARE_PROPERTY
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of PROPERTY
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of PROPERTY
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of PROPERTY
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECLARE_STATE
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of STATE
##### inner_range
Type: string
Initial Value: ctx.range(code.ref().value())
Description: range of STATE
##### ident
Type: string
Initial Value: ctx.ident(ref)
Description: identifier of STATE
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DECLARE_UNION
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### format_nested_struct
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:93
Function Name: operator()
### DECLARE_UNION_MEMBER
#### Variables: 
##### ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of UNION_MEMBER
##### inner_range
Type: string
Initial Value: ctx.range(ref)
Description: range of UNION_MEMBER
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### format_nested_struct
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:93
Function Name: operator()
### DEFINE_BIT_FIELD
#### Variables: 
##### type
Type: string
Initial Value: type_to_string(ctx,code.type().value())
Description: bit field type
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of bit field
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of bit field
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belonging struct or bit field
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### compact_bit_field
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:168
Function Name: operator()
### DEFINE_ENUM
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
##### base_type_ref
Type: StorageRef
Initial Value: code.type().value()
Description: type reference of enum base type
##### base_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: enum base type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### default_enum_base
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:141
Function Name: write_inner_block
##### enum_keyword
Flag Value: `enum`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:147
Function Name: operator()
##### enum_base_separator
Flag Value: ` : `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:149
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:151
Function Name: operator()
### DEFINE_ENUM_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of enum member
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum member
##### evaluated_ref
Type: Varint
Initial Value: code.left_ref().value()
Description: reference of enum member value
##### evaluated
Type: EvalResult
Initial Value: eval(ctx.ref(evaluated_ref), ctx)
Description: enum member value
##### enum_ident_ref
Type: Varint
Initial Value: code.belong().value()
Description: reference of enum
##### enum_ident
Type: string
Initial Value: ctx.ident(enum_ident_ref)
Description: identifier of enum
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### enum_member_end
Flag Value: `,`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:160
Function Name: operator()
### DEFINE_FIELD
#### Variables: 
##### type
Type: string
Initial Value: type_to_string(ctx,code.type().value())
Description: field type
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of field
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of field
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belonging struct or bit field
##### is_bit_field
Type: bool
Initial Value: belong.value()!=0&&ctx.ref(belong).op==rebgn::AbstractOp::DEFINE_BIT_FIELD
Description: is part of bit field
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### compact_bit_field
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:202
Function Name: operator()
##### define_field
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:206
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:215
Function Name: operator()
##### field_end
Flag Value: `;`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:219
Function Name: operator()
##### field_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:219
Function Name: operator()
##### compact_bit_field
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:222
Function Name: operator()
### DEFINE_FORMAT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
##### is_empty_block
Type: bool
Initial Value: range.start ==range.end - 2
Description: is empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
##### struct_keyword
Flag Value: `struct`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
### DEFINE_PROPERTY_GETTER
#### Variables: 
##### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
##### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
##### ident_ref
Type: Varint
Initial Value: ctx.ref(func).ident().value()
Description: reference of function
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of function
##### ret_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
##### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### format_nested_function
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:252
Function Name: operator()
### DEFINE_PROPERTY_SETTER
#### Variables: 
##### func
Type: Varint
Initial Value: code.right_ref().value()
Description: reference of function
##### inner_range
Type: string
Initial Value: ctx.range(func)
Description: range of function
##### ident_ref
Type: Varint
Initial Value: ctx.ref(func).ident().value()
Description: reference of function
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of function
##### ret_type
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function return type
##### belong_name
Type: std::optional<std::string>
Initial Value: std::nullopt
Description: function belong name
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### format_nested_function
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:252
Function Name: operator()
### DEFINE_STATE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
##### is_empty_block
Type: bool
Initial Value: range.start ==range.end - 2
Description: is empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
##### struct_keyword
Flag Value: `struct`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of union
##### is_empty_block
Type: bool
Initial Value: range.start ==range.end - 2
Description: is empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### variant_mode
Flag Value: `union`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:118
Function Name: operator()
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:119
Function Name: operator()
##### union_keyword
Flag Value: `union`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:119
Function Name: operator()
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of format
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of format
##### is_empty_block
Type: bool
Initial Value: range.start ==range.end - 2
Description: is empty block
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_begin
Flag Value: `{`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
##### struct_keyword
Flag Value: `struct`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:110
Function Name: operator()
### END_ENUM
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end_type
Flag Value: `};`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:237
Function Name: operator()
### END_FORMAT
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end_type
Flag Value: `};`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:237
Function Name: operator()
### END_STATE
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end_type
Flag Value: `};`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:237
Function Name: operator()
### END_UNION
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### variant_mode
Flag Value: `union`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:126
Function Name: operator()
##### block_end_type
Flag Value: `};`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:128
Function Name: operator()
### END_UNION_MEMBER
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### block_end_type
Flag Value: `};`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/inner_block.cpp:237
Function Name: operator()
## function `add_parameter`
### DECODER_PARAMETER
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### decoder_param
Flag Value: `Decoder& r`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:67
Function Name: operator()
### DEFINE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### define_parameter
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:42
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:50
Function Name: operator()
##### param_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:54
Function Name: operator()
### ENCODER_PARAMETER
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### encoder_param
Flag Value: `Encoder& w`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:61
Function Name: operator()
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
##### type_ref
Type: Varint
Initial Value: code.type().value()
Description: reference of parameter type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: parameter type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### define_parameter
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:42
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:50
Function Name: operator()
##### param_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:54
Function Name: operator()
### STATE_VARIABLE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
##### type_ref
Type: Varint
Initial Value: ctx.ref(ident_ref).type().value()
Description: reference of state variable type
##### type
Type: string
Initial Value: type_to_string(ctx,type_ref)
Description: state variable type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### define_parameter
Flag Value: ``
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:42
Function Name: operator()
##### prior_ident
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:50
Function Name: operator()
##### param_type_separator
Flag Value: ` `
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/parameter.cpp:54
Function Name: operator()
## function `add_call_parameter`
### PROPERTY_INPUT_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of parameter
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of parameter
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### STATE_VARIABLE_PARAMETER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ref().value()
Description: reference of state variable
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of state variable
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
## function `type_to_string`
### ARRAY
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
##### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
##### length
Type: uint64_t
Initial Value: storage.size()->value()
Description: array length
#### Placeholder Mappings: 
##### array_type
Name: LEN
Mapped Value: `",futils::number::to_string<std::string>(length),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:166
Function Name: operator()
Name: TYPE
Mapped Value: `",base_type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:166
Function Name: operator()
##### byte_array_type
Name: LEN
Mapped Value: `",futils::number::to_string<std::string>(length),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:170
Function Name: operator()
Name: TYPE
Mapped Value: `",base_type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:170
Function Name: operator()
#### Flag Usage Mappings: 
##### byte_array_type
Flag Value: `std::array<std::uint8_t, $LEN>`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:167
Function Name: operator()
### BOOL
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### bool_type
Flag Value: `bool`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:78
Function Name: operator()
### CODER_RETURN
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### coder_return_type
Flag Value: `bool`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:141
Function Name: operator()
### DEFINE_FUNCTION
#### Variables: 
##### func_type
Type: rebgn::FunctionType
Initial Value: code.func_type().value()
Description: function type
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_PROGRAM
#### Variables: 
#### Placeholder Mappings: 
##### keyword_escape_style
Name: VALUE
Mapped Value: `",str,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/main.cpp:151
Function Name: write_code_template
#### Flag Usage Mappings: 
### ENUM
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of enum
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of enum
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### FLOAT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size <= 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### float_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:47
Function Name: operator()
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:47
Function Name: operator()
#### Flag Usage Mappings: 
### INT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size <= 8 ? 8 : size <= 16 ? 16 : size <= 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### int_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:51
Function Name: operator()
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:51
Function Name: operator()
#### Flag Usage Mappings: 
### OPTIONAL
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### Placeholder Mappings: 
##### optional_type
Name: TYPE
Mapped Value: `",base_type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:200
Function Name: operator()
#### Flag Usage Mappings: 
### PROPERTY_SETTER_RETURN
#### Variables: 
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### property_setter_return_type
Flag Value: `bool`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:146
Function Name: operator()
### PTR
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
#### Placeholder Mappings: 
##### pointer_type
Name: TYPE
Mapped Value: `",base_type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:154
Function Name: operator()
#### Flag Usage Mappings: 
### RECURSIVE_STRUCT_REF
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of recursive struct
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of recursive struct
#### Placeholder Mappings: 
##### recursive_struct_type
Name: TYPE
Mapped Value: `",ident,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:72
Function Name: operator()
#### Flag Usage Mappings: 
### STRUCT_REF
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of struct
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of struct
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### UINT
#### Variables: 
##### size
Type: uint64_t
Initial Value: storage.size()->value()
Description: bit size
##### aligned_size
Type: uint64_t
Initial Value: size <= 8 ? 8 : size <= 16 ? 16 : size <= 32 ? 32 : 64
Description: aligned bit size
#### Placeholder Mappings: 
##### uint_type
Name: ALIGNED_BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(aligned_size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:55
Function Name: operator()
Name: BIT_SIZE
Mapped Value: `",futils::number::to_string<std::string>(size),"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:55
Function Name: operator()
#### Flag Usage Mappings: 
### VARIANT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: storage.ref().value()
Description: reference of variant
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of variant
##### types
Type: std::vector<std::string>
Initial Value: {}
Description: variant types
##### variant_size
Type: std::optional<size_t>
Initial Value: 0
Description: variant storage size
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
##### variant_mode
Flag Value: `union`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:115
Function Name: operator()
### VECTOR
#### Variables: 
##### base_type
Type: string
Initial Value: type_to_string_impl(ctx, s, bit_size, index + 1)
Description: base type
##### is_byte_vector
Type: bool
Initial Value: index + 1 < s.storages.size() && s.storages[index + 1].type == rebgn::StorageType::UINT && s.storages[index + 1].size().value().value() == 8
Description: is byte vector
#### Placeholder Mappings: 
##### vector_type
Name: TYPE
Mapped Value: `",base_type,"`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:191
Function Name: operator()
#### Flag Usage Mappings: 
##### byte_vector_type
Flag Value: `std::vector<std::uint8_t>`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:181
Function Name: operator()
##### byte_vector_type
Flag Value: `std::vector<std::uint8_t>`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/type.cpp:184
Function Name: operator()
## function `field_accessor`
### DEFINE_BIT_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `belong_eval`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
##### compact_bit_field
Flag Value: `false`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/accessor.cpp:88
Function Name: operator()
### DEFINE_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_FORMAT
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ctx.this_()`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_PROPERTY
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_STATE
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ctx.this_()`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: field_accessor(ctx.ref(belong), ctx)
Description: belong eval
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}.{}", belong_eval.result, ident)`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `ident`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
##### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
##### union_field_eval
Type: string
Initial Value: field_accessor(ctx.ref(union_field_ref),ctx)
Description: field accessor
#### Placeholder Mappings: 
##### eval_result_passthrough
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `union_field_eval`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:26
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
## function `type_accessor`
### DEFINE_BIT_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of BIT_FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of BIT_FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_FIELD
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FIELD
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FIELD
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_FORMAT
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of FORMAT
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of FORMAT
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_PROGRAM
#### Variables: 
#### Placeholder Mappings: 
##### eval_result_text
Name: RESULT
Mapped Value: `result`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
Name: TEXT
Mapped Value: `std::format("{}{}{}","/*",to_string(code.op),"*/")`
File: C:/workspace/shbrgen/rebrgen/src/old/bm2/gen_template/eval.cpp:30
Function Name: do_make_eval_result
#### Flag Usage Mappings: 
### DEFINE_PROPERTY
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of PROPERTY
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of PROPERTY
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_STATE
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of STATE
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of STATE
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_UNION
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
### DEFINE_UNION_MEMBER
#### Variables: 
##### ident_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of UNION_MEMBER
##### ident
Type: string
Initial Value: ctx.ident(ident_ref)
Description: identifier of UNION_MEMBER
##### belong
Type: Varint
Initial Value: code.belong().value()
Description: reference of belong
##### is_member
Type: bool
Initial Value: belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM
Description: is member of a struct
##### union_member_ref
Type: Varint
Initial Value: code.ident().value()
Description: reference of union member
##### union_ref
Type: Varint
Initial Value: belong
Description: reference of union
##### union_field_ref
Type: Varint
Initial Value: ctx.ref(union_ref).belong().value()
Description: reference of union field
##### union_field_belong
Type: Varint
Initial Value: ctx.ref(union_field_ref).belong().value()
Description: reference of union field belong
##### belong_eval
Type: string
Initial Value: type_accessor(ctx.ref(union_field_belong),ctx)
Description: field accessor
#### Placeholder Mappings: 
#### Flag Usage Mappings: 
