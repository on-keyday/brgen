## AbstractOp Arguments

This document describes the arguments for each `AbstractOp` defined in `src/old/bm/binary_module.bgn`.

```
enum AbstractOp:
    :u8


    METADATA
    IMPORT
    DYNAMIC_ENDIAN
    RETURN_TYPE

    DEFINE_PROGRAM
    END_PROGRAM
    DECLARE_PROGRAM

    DEFINE_FORMAT
    END_FORMAT
    DECLARE_FORMAT

    DEFINE_FIELD
    CONDITIONAL_FIELD
    CONDITIONAL_PROPERTY
    MERGED_CONDITIONAL_FIELD

    DEFINE_PROPERTY
    END_PROPERTY
    DECLARE_PROPERTY

    DEFINE_PROPERTY_SETTER
    DEFINE_PROPERTY_GETTER

    DEFINE_PARAMETER

    DEFINE_FUNCTION
    END_FUNCTION
    DECLARE_FUNCTION

    DEFINE_ENUM
    END_ENUM
    DECLARE_ENUM

    DEFINE_ENUM_MEMBER

    DEFINE_UNION
    END_UNION
    DECLARE_UNION

    DEFINE_UNION_MEMBER
    END_UNION_MEMBER
    DECLARE_UNION_MEMBER

    DEFINE_STATE
    END_STATE
    DECLARE_STATE

    DEFINE_BIT_FIELD
    END_BIT_FIELD
    DECLARE_BIT_FIELD

    BEGIN_ENCODE_PACKED_OPERATION
    END_ENCODE_PACKED_OPERATION
    BEGIN_DECODE_PACKED_OPERATION
    END_DECODE_PACKED_OPERATION

    DEFINE_ENCODER

    DEFINE_DECODER

    ENCODE_INT
    DECODE_INT
    ENCODE_INT_VECTOR
    ENCODE_INT_VECTOR_FIXED # must use length for fixed size (for example, alignment sub byte)
    DECODE_INT_VECTOR
    DECODE_INT_VECTOR_UNTIL_EOF
    DECODE_INT_VECTOR_FIXED # must use length for fixed size (for example, alignment sub byte)
    PEEK_INT_VECTOR
    BACKWARD_INPUT
    BACKWARD_OUTPUT
    INPUT_BYTE_OFFSET
    OUTPUT_BYTE_OFFSET
    INPUT_BIT_OFFSET
    OUTPUT_BIT_OFFSET
    REMAIN_BYTES
    CAN_READ
    IS_LITTLE_ENDIAN # whether platform is little endian (for native endian support)
    CALL_ENCODE
    CALL_DECODE

    CAST
    CALL_CAST # for Cast with multi argument ast node

    ADDRESS_OF
    OPTIONAL_OF
    EMPTY_PTR
    EMPTY_OPTIONAL

    LOOP_INFINITE
    LOOP_CONDITION
    CONTINUE
    BREAK
    END_LOOP

    IF
    ELIF
    ELSE
    END_IF

    MATCH
    EXHAUSTIVE_MATCH
    CASE
    END_CASE
    DEFAULT_CASE
    END_MATCH

    DEFINE_VARIABLE
    DEFINE_VARIABLE_REF
    DEFINE_CONSTANT
    DECLARE_VARIABLE

    BINARY
    NOT_PREV_THEN
    UNARY
    ASSIGN
    PROPERTY_ASSIGN

    ASSERT
    LENGTH_CHECK
    EXPLICIT_ERROR

    ACCESS

    INDEX
    APPEND
    INC
    CALL
    RET
    RET_SUCCESS
    RET_PROPERTY_SETTER_OK
    RET_PROPERTY_SETTER_FAIL

    IMMEDIATE_TRUE
    IMMEDIATE_FALSE
    IMMEDIATE_INT
    IMMEDIATE_INT64
    IMMEDIATE_CHAR
    IMMEDIATE_STRING
    IMMEDIATE_TYPE

    NEW_OBJECT
    INIT_RECURSIVE_STRUCT
    CHECK_RECURSIVE_STRUCT


    SWITCH_UNION
    CHECK_UNION

    ENCODER_PARAMETER
    DECODER_PARAMETER

    STATE_VARIABLE_PARAMETER

    PROPERTY_INPUT_PARAMETER

    EVAL_EXPR

    ARRAY_SIZE
    RESERVE_SIZE

    BEGIN_ENCODE_SUB_RANGE
    END_ENCODE_SUB_RANGE
    BEGIN_DECODE_SUB_RANGE
    END_DECODE_SUB_RANGE

    SEEK_ENCODER
    SEEK_DECODER

    FIELD_AVAILABLE

    PHI # for assignment analysis

    PROPERTY_FUNCTION # marker for property function

    DEFINE_FALLBACK
    END_FALLBACK

    BEGIN_COND_BLOCK # marker for conditional block
    END_COND_BLOCK
```

### METADATA

- `metadata`: `Metadata` - Metadata information.

### IMPORT

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to program.

### DYNAMIC_ENDIAN

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to endian expression.
- `fallback`: `Varint` - Reference to fallback block.

### RETURN_TYPE

- `type`: `StorageRef` - Reference to storage type.

### DEFINE_PROGRAM

- `ident`: `Varint` - Identifier.

### END_PROGRAM

_(No arguments)_

### DECLARE_PROGRAM

- `ref`: `Varint` - Reference to program.

### DEFINE_FORMAT

- `ident`: `Varint` - Identifier.

### END_FORMAT

_(No arguments)_

### DECLARE_FORMAT

- `ref`: `Varint` - Reference to format.

### DEFINE_FIELD

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to format, state, union, or null.
- `type`: `StorageRef` - Reference to storage type.

### CONDITIONAL_FIELD

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to expression.
- `right_ref`: `Varint` - Reference to field.
- `belong`: `Varint` - Belongs to union field.

### CONDITIONAL_PROPERTY

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to expression.
- `right_ref`: `Varint` - Reference to merged conditional field.
- `belong`: `Varint` - Belongs to union field.

### MERGED_CONDITIONAL_FIELD

- `ident`: `Varint` - Identifier.
- `type`: `StorageRef` - Reference to storage type.
- `param`: `Param` - Reference to conditional fields.
- `belong`: `Varint` - Belongs to property.
- `merge_mode`: `MergeMode` - Merge mode.

### DEFINE_PROPERTY

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to format, state, or union, or null.

### END_PROPERTY

_(No arguments)_

### DECLARE_PROPERTY

- `ref`: `Varint` - Reference to property.

### DEFINE_PROPERTY_SETTER

- `left_ref`: `Varint` - Reference to merged conditional field.
- `right_ref`: `Varint` - Reference to function.

### DEFINE_PROPERTY_GETTER

- `left_ref`: `Varint` - Reference to merged conditional field.
- `right_ref`: `Varint` - Reference to function.

### DEFINE_PARAMETER

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to function.
- `type`: `StorageRef` - Reference to storage type.

### DEFINE_FUNCTION

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to format or null.
- `func_type`: `FunctionType` - Function type.

### END_FUNCTION

_(No arguments)_

### DECLARE_FUNCTION

- `ref`: `Varint` - Reference to function.

### DEFINE_ENUM

- `ident`: `Varint` - Identifier.
- `type`: `StorageRef` - Base type or null.

### END_ENUM

_(No arguments)_

### DECLARE_ENUM

- `ref`: `Varint` - Reference to enum.

### DEFINE_ENUM_MEMBER

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to init expression.
- `right_ref`: `Varint` - Reference to string representation or null.

### DEFINE_UNION

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Reference to field.

### END_UNION

_(No arguments)_

### DECLARE_UNION

- `ref`: `Varint` - Reference to union.

### DEFINE_UNION_MEMBER

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to union.

### END_UNION_MEMBER

_(No arguments)_

### DECLARE_UNION_MEMBER

- `ref`: `Varint` - Reference to union member.

### DEFINE_STATE

- `ident`: `Varint` - Identifier.

### END_STATE

_(No arguments)_

### DECLARE_STATE

- `ref`: `Varint` - Reference to state.

### DEFINE_BIT_FIELD

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Belongs to format, state, union, or null.
- `type`: `StorageRef` - Bit field sum type.

### END_BIT_FIELD

_(No arguments)_

### DECLARE_BIT_FIELD

- `ref`: `Varint` - Reference to bit field.

### BEGIN_ENCODE_PACKED_OPERATION

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Reference to bit field.
- `packed_op_type`: `PackedOpType` - Packed operation type.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `fallback`: `Varint` - Reference to fallback block.

### END_ENCODE_PACKED_OPERATION

- `fallback`: `Varint` - Reference to fallback block.

### BEGIN_DECODE_PACKED_OPERATION

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Reference to bit field.
- `packed_op_type`: `PackedOpType` - Packed operation type.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `fallback`: `Varint` - Reference to fallback block.

### END_DECODE_PACKED_OPERATION

- `fallback`: `Varint` - Reference to fallback block.

### DEFINE_ENCODER

- `left_ref`: `Varint` - Reference to format.
- `right_ref`: `Varint` - Reference to encoder.

### DEFINE_DECODER

- `left_ref`: `Varint` - Reference to format.
- `right_ref`: `Varint` - Reference to decoder.

### ENCODE_INT

- `ref`: `Varint` - Reference to value to encode.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `fallback`: `Varint` - Reference to fallback block.

### DECODE_INT

- `ref`: `Varint` - Reference to value to decode.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `fallback`: `Varint` - Reference to fallback block.

### ENCODE_INT_VECTOR

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `fallback`: `Varint` - Reference to fallback block.

### ENCODE_INT_VECTOR_FIXED

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `array_length`: `Varint` - Array length.
- `fallback`: `Varint` - Reference to fallback block.

### DECODE_INT_VECTOR

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `fallback`: `Varint` - Reference to fallback block.

### DECODE_INT_VECTOR_FIXED

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `array_length`: `Varint` - Array length.
- `fallback`: `Varint` - Reference to fallback block.

### DECODE_INT_VECTOR_UNTIL_EOF

- `ref`: `Varint` - Reference to vector.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.
- `fallback`: `Varint` - Reference to fallback block.

### PEEK_INT_VECTOR

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `endian`: `EndianExpr` - Endianness expression.
- `bit_size`: `Varint` - Bit size.
- `belong`: `Varint` - Related field or null.

### BACKWARD_INPUT

- `ref`: `Varint` - Bit count.

### BACKWARD_OUTPUT

- `ref`: `Varint` - Bit count.

### INPUT_BYTE_OFFSET

- `ident`: `Varint` - Identifier.

### OUTPUT_BYTE_OFFSET

- `ident`: `Varint` - Identifier.

### INPUT_BIT_OFFSET

- `ident`: `Varint` - Identifier.

### OUTPUT_BIT_OFFSET

- `ident`: `Varint` - Identifier.

### REMAIN_BYTES

- `ident`: `Varint` - Identifier.

### CAN_READ

- `ident`: `Varint` - Identifier.
- `belong`: `Varint` - Related field.

### IS_LITTLE_ENDIAN

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to dynamic endian variable (dynamic) or null (platform).
- `fallback`: `Varint` - Reference to fallback boolean expression.

### CALL_ENCODE

- `left_ref`: `Varint` - Reference to encoder.
- `right_ref`: `Varint` - Reference to object.
- `bit_size_plus`: `Varint` - Bit size plus 1 (0 means null).

### CALL_DECODE

- `left_ref`: `Varint` - Reference to decoder.
- `right_ref`: `Varint` - Reference to object.
- `bit_size_plus`: `Varint` - Bit size plus 1 (0 means null).

### CAST

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to object.
- `type`: `StorageRef` - Reference to storage type.
- `from_type`: `StorageRef` - Reference to storage type.
- `cast_type`: `CastType` - Cast type.

### CALL_CAST

- `ident`: `Varint` - Identifier.
- `type`: `StorageRef` - Reference to storage type.
- `param`: `Param` - Parameters.

### ADDRESS_OF

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to object.

### OPTIONAL_OF

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to object.
- `type`: `StorageRef` - Reference to storage type.

### EMPTY_PTR

- `ident`: `Varint` - Identifier.

### EMPTY_OPTIONAL

- `ident`: `Varint` - Identifier.

### LOOP_INFINITE

_(No arguments)_

### LOOP_CONDITION

- `ref`: `Varint` - Reference to expression.

### CONTINUE

_(No arguments)_

### BREAK

_(No arguments)_

### END_LOOP

_(No arguments)_

### IF

- `ref`: `Varint` - Reference to expression.

### ELIF

- `ref`: `Varint` - Reference to expression.

### ELSE

_(No arguments)_

### END_IF

_(No arguments)_

### MATCH

- `ref`: `Varint` - Reference to expression.

### EXHAUSTIVE_MATCH

- `ref`: `Varint` - Reference to expression.

### CASE

- `ref`: `Varint` - Reference to expression.

### END_CASE

_(No arguments)_

### DEFAULT_CASE

_(No arguments)_

### END_MATCH

_(No arguments)_

### DEFINE_VARIABLE

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to initial expression.
- `type`: `StorageRef` - Reference to storage type.

### DEFINE_VARIABLE_REF

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to variable.

### DEFINE_CONSTANT

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to initial expression.
- `type`: `StorageRef` - Reference to storage type.

### DECLARE_VARIABLE

- `ref`: `Varint` - Reference to variable.

### BINARY

- `ident`: `Varint` - Identifier.
- `bop`: `BinaryOp` - Binary operator.
- `left_ref`: `Varint` - Reference to left expression.
- `right_ref`: `Varint` - Reference to right expression.

### NOT_PREV_THEN

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to previous expression.
- `right_ref`: `Varint` - Reference to then expression.

### UNARY

- `ident`: `Varint` - Identifier.
- `uop`: `UnaryOp` - Unary operator.
- `ref`: `Varint` - Reference to expression.

### ASSIGN

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to previous definition, assignment, or phi.
- `left_ref`: `Varint` - Reference to variable.
- `right_ref`: `Varint` - Reference to expression.

### PROPERTY_ASSIGN

- `left_ref`: `Varint` - Reference to property setter.
- `right_ref`: `Varint` - Reference to expression.

### ADDRESS_OF

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to object.

### OPTIONAL_OF

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to object.
- `type`: `StorageRef` - Reference to storage type.

### EMPTY_PTR

- `ident`: `Varint` - Identifier.

### EMPTY_OPTIONAL

- `ident`: `Varint` - Identifier.

### ASSERT

- `ref`: `Varint` - Reference to condition expression.
- `belong`: `Varint` - Belongs to function.

### LENGTH_CHECK

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to length.
- `belong`: `Varint` - Belongs to function.

### EXPLICIT_ERROR

- `param`: `Param` - Reference to error messages.
- `belong`: `Varint` - Belongs to function.

### ACCESS

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to base expression.
- `right_ref`: `Varint` - Reference to member name.

### INDEX

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to base expression.
- `right_ref`: `Varint` - Reference to index expression.

### APPEND

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to object.

### INC

- `ref`: `Varint` - Reference to variable to increment.

### CALL

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to function.
- `param`: `Param` - Parameters.

### RET

- `belong`: `Varint` - Belongs to function.
- `ref`: `Varint` - Reference to expression (maybe null).

### RET_SUCCESS

- `belong`: `Varint` - Belongs to function.

### RET_PROPERTY_SETTER_OK

- `belong`: `Varint` - Belongs to property setter.

### RET_PROPERTY_SETTER_FAIL

- `belong`: `Varint` - Belongs to property setter.

### IMMEDIATE_TRUE

- `ident`: `Varint` - Identifier.

### IMMEDIATE_FALSE

- `ident`: `Varint` - Identifier.

### IMMEDIATE_INT

- `ident`: `Varint` - Identifier.
- `int_value`: `Varint` - Integer value.

### IMMEDIATE_INT64

- `ident`: `Varint` - Identifier.
- `int_value64`: `u64` - 64-bit integer value.

### IMMEDIATE_CHAR

- `ident`: `Varint` - Identifier.
- `int_value`: `Varint` - Integer value representing the character.

### IMMEDIATE_STRING

- `ident`: `Varint` - Identifier.

### IMMEDIATE_TYPE

- `ident`: `Varint` - Identifier.
- `type`: `StorageRef` - Reference to storage type.

### NEW_OBJECT

- `ident`: `Varint` - Identifier.
- `type`: `StorageRef` - Reference to storage type.

### INIT_RECURSIVE_STRUCT

- `left_ref`: `Varint` - Reference to recursive struct (format).
- `right_ref`: `Varint` - Reference to object.

### CHECK_RECURSIVE_STRUCT

- `left_ref`: `Varint` - Reference to recursive struct (format).
- `right_ref`: `Varint` - Reference to object.

### SWITCH_UNION

- `ref`: `Varint` - Reference to union_member.

### CHECK_UNION

- `ref`: `Varint` - Reference to union_member.
- `check_at`: `UnionCheckAt` - When to check the union.

### ENCODER_PARAMETER

- `left_ref`: `Varint` - Reference to format.
- `right_ref`: `Varint` - Reference to encoder function.
- `encode_flags`: `EncodeParamFlags` - Encoder parameter flags.

### DECODER_PARAMETER

- `left_ref`: `Varint` - Reference to format.
- `right_ref`: `Varint` - Reference to decoder function.
- `decode_flags`: `DecodeParamFlags` - Decoder parameter flags.

### STATE_VARIABLE_PARAMETER

- `ref`: `Varint` - Reference to state.

### PROPERTY_INPUT_PARAMETER

- `ident`: `Varint` - Identifier.
- `left_ref`: `Varint` - Reference to merged conditional field (for union) or field (for dynamic array).
- `right_ref`: `Varint` - Reference to function.
- `type`: `StorageRef` - Reference to storage type.

### PROPERTY_FUNCTION

- `ref`: `Varint` - Reference to merged conditional field.

### DEFINE_PROPERTY_SETTER

- `left_ref`: `Varint` - Reference to merged conditional field.
- `right_ref`: `Varint` - Reference to function.

### DEFINE_PROPERTY_GETTER

- `left_ref`: `Varint` - Reference to merged conditional field.
- `right_ref`: `Varint` - Reference to function.

### PHI

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to original variable.
- `phi_params`: `PhiParams` - Reference to previous definitions, assignments, or phis.

### EVAL_EXPR

- `ref`: `Varint` - Reference to expression.

### ARRAY_SIZE

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to array.

### RESERVE_SIZE

- `left_ref`: `Varint` - Reference to vector.
- `right_ref`: `Varint` - Reference to size.

### BEGIN_ENCODE_SUB_RANGE

- `sub_range_type`: `SubRangeType` - Sub-range type.
- `ref`: `Varint` - Length of sub-range or vector.
- `belong`: `Varint` - Related field.

### BEGIN_DECODE_SUB_RANGE

- `sub_range_type`: `SubRangeType` - Sub-range type.
- `ref`: `Varint` - Length of sub-range.
- `belong`: `Varint` - Related field.

### SEEK_ENCODER

- `ref`: `Varint` - Reference to offset.
- `belong`: `Varint` - Related field.

### SEEK_DECODER

- `ref`: `Varint` - Reference to offset.
- `belong`: `Varint` - Related field.

### DEFINE_FALLBACK

- `ident`: `Varint` - Identifier.

### END_FALLBACK

_(No arguments)_

### BEGIN_COND_BLOCK

- `ident`: `Varint` - Identifier.
- `ref`: `Varint` - Reference to condition expression.

### END_COND_BLOCK

_(No arguments)_
