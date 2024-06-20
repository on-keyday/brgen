// Code generated by gen_ast2c.go. DO NOT EDIT.

#pragma once
#ifndef __AST_H__
#define __AST_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif


typedef struct ast2c_json_handlers ast2c_json_handlers;
struct ast2c_json_handlers {
	void* ctx;
	void* (*object_get)(ast2c_json_handlers*, void* obj, const char* name);
	void* (*array_get)(ast2c_json_handlers*, void* obj, size_t i);
	// return non-zero for success, 0 for error
	int (*array_size)(ast2c_json_handlers*, void* obj, size_t* size);
	int (*is_null)(ast2c_json_handlers*, void* obj);
	int (*is_array)(ast2c_json_handlers*, void* obj);
	int (*is_object)(ast2c_json_handlers*, void* obj);
	char* (*string_get_alloc)(ast2c_json_handlers*, void* obj);
	const char* (*string_get)(ast2c_json_handlers*, void* obj);
	// returns non-zero for success, 0 for error
	int (*number_get)(ast2c_json_handlers*, void* obj, uint64_t* n);
	// returns 0 or 1. -1 for error
	int (*boolean_get)(ast2c_json_handlers*, void* obj);

	void* (*alloc)(ast2c_json_handlers*, size_t size, size_t align);
	void (*free)(ast2c_json_handlers*, void* ptr);

	void (*error)(ast2c_json_handlers*, void* ptr, const char* msg);
};

typedef struct ast2c_Ast ast2c_Ast;
typedef enum ast2c_NodeType ast2c_NodeType;
typedef enum ast2c_TokenTag ast2c_TokenTag;
typedef enum ast2c_UnaryOp ast2c_UnaryOp;
typedef enum ast2c_BinaryOp ast2c_BinaryOp;
typedef enum ast2c_IdentUsage ast2c_IdentUsage;
typedef enum ast2c_Endian ast2c_Endian;
typedef enum ast2c_ConstantLevel ast2c_ConstantLevel;
typedef enum ast2c_BitAlignment ast2c_BitAlignment;
typedef enum ast2c_Follow ast2c_Follow;
typedef enum ast2c_IoMethod ast2c_IoMethod;
typedef enum ast2c_SpecialLiteralKind ast2c_SpecialLiteralKind;
typedef enum ast2c_OrderType ast2c_OrderType;
typedef enum ast2c_FormatTrait ast2c_FormatTrait;
typedef struct ast2c_Node ast2c_Node;
typedef struct ast2c_Expr ast2c_Expr;
typedef struct ast2c_Stmt ast2c_Stmt;
typedef struct ast2c_Type ast2c_Type;
typedef struct ast2c_Literal ast2c_Literal;
typedef struct ast2c_Member ast2c_Member;
typedef struct ast2c_Program ast2c_Program;
typedef struct ast2c_Comment ast2c_Comment;
typedef struct ast2c_CommentGroup ast2c_CommentGroup;
typedef struct ast2c_FieldArgument ast2c_FieldArgument;
typedef struct ast2c_Binary ast2c_Binary;
typedef struct ast2c_Unary ast2c_Unary;
typedef struct ast2c_Cond ast2c_Cond;
typedef struct ast2c_Ident ast2c_Ident;
typedef struct ast2c_Call ast2c_Call;
typedef struct ast2c_If ast2c_If;
typedef struct ast2c_MemberAccess ast2c_MemberAccess;
typedef struct ast2c_Paren ast2c_Paren;
typedef struct ast2c_Index ast2c_Index;
typedef struct ast2c_Match ast2c_Match;
typedef struct ast2c_Range ast2c_Range;
typedef struct ast2c_Identity ast2c_Identity;
typedef struct ast2c_TmpVar ast2c_TmpVar;
typedef struct ast2c_Import ast2c_Import;
typedef struct ast2c_Cast ast2c_Cast;
typedef struct ast2c_Available ast2c_Available;
typedef struct ast2c_SpecifyOrder ast2c_SpecifyOrder;
typedef struct ast2c_ExplicitError ast2c_ExplicitError;
typedef struct ast2c_IoOperation ast2c_IoOperation;
typedef struct ast2c_OrCond ast2c_OrCond;
typedef struct ast2c_BadExpr ast2c_BadExpr;
typedef struct ast2c_Loop ast2c_Loop;
typedef struct ast2c_IndentBlock ast2c_IndentBlock;
typedef struct ast2c_ScopedStatement ast2c_ScopedStatement;
typedef struct ast2c_MatchBranch ast2c_MatchBranch;
typedef struct ast2c_UnionCandidate ast2c_UnionCandidate;
typedef struct ast2c_Return ast2c_Return;
typedef struct ast2c_Break ast2c_Break;
typedef struct ast2c_Continue ast2c_Continue;
typedef struct ast2c_Assert ast2c_Assert;
typedef struct ast2c_ImplicitYield ast2c_ImplicitYield;
typedef struct ast2c_Metadata ast2c_Metadata;
typedef struct ast2c_IntType ast2c_IntType;
typedef struct ast2c_FloatType ast2c_FloatType;
typedef struct ast2c_IdentType ast2c_IdentType;
typedef struct ast2c_IntLiteralType ast2c_IntLiteralType;
typedef struct ast2c_StrLiteralType ast2c_StrLiteralType;
typedef struct ast2c_RegexLiteralType ast2c_RegexLiteralType;
typedef struct ast2c_VoidType ast2c_VoidType;
typedef struct ast2c_BoolType ast2c_BoolType;
typedef struct ast2c_ArrayType ast2c_ArrayType;
typedef struct ast2c_FunctionType ast2c_FunctionType;
typedef struct ast2c_StructType ast2c_StructType;
typedef struct ast2c_StructUnionType ast2c_StructUnionType;
typedef struct ast2c_UnionType ast2c_UnionType;
typedef struct ast2c_RangeType ast2c_RangeType;
typedef struct ast2c_EnumType ast2c_EnumType;
typedef struct ast2c_MetaType ast2c_MetaType;
typedef struct ast2c_OptionalType ast2c_OptionalType;
typedef struct ast2c_GenericType ast2c_GenericType;
typedef struct ast2c_IntLiteral ast2c_IntLiteral;
typedef struct ast2c_BoolLiteral ast2c_BoolLiteral;
typedef struct ast2c_StrLiteral ast2c_StrLiteral;
typedef struct ast2c_RegexLiteral ast2c_RegexLiteral;
typedef struct ast2c_CharLiteral ast2c_CharLiteral;
typedef struct ast2c_TypeLiteral ast2c_TypeLiteral;
typedef struct ast2c_SpecialLiteral ast2c_SpecialLiteral;
typedef struct ast2c_Field ast2c_Field;
typedef struct ast2c_Format ast2c_Format;
typedef struct ast2c_State ast2c_State;
typedef struct ast2c_Enum ast2c_Enum;
typedef struct ast2c_EnumMember ast2c_EnumMember;
typedef struct ast2c_Function ast2c_Function;
typedef struct ast2c_Scope ast2c_Scope;
typedef struct ast2c_Pos ast2c_Pos;
typedef struct ast2c_Loc ast2c_Loc;
typedef struct ast2c_Token ast2c_Token;
typedef struct ast2c_RawScope ast2c_RawScope;
typedef struct ast2c_RawNode ast2c_RawNode;
typedef struct ast2c_SrcErrorEntry ast2c_SrcErrorEntry;
typedef struct ast2c_SrcError ast2c_SrcError;
typedef struct ast2c_JsonAst ast2c_JsonAst;
typedef struct ast2c_AstFile ast2c_AstFile;
typedef struct ast2c_TokenFile ast2c_TokenFile;
enum ast2c_NodeType {
	AST2C_NODETYPE_PROGRAM = 1,
	AST2C_NODETYPE_COMMENT = 2,
	AST2C_NODETYPE_COMMENT_GROUP = 3,
	AST2C_NODETYPE_FIELD_ARGUMENT = 4,
	AST2C_NODETYPE_EXPR = 65536,
	AST2C_NODETYPE_BINARY = 65537,
	AST2C_NODETYPE_UNARY = 65538,
	AST2C_NODETYPE_COND = 65539,
	AST2C_NODETYPE_IDENT = 65540,
	AST2C_NODETYPE_CALL = 65541,
	AST2C_NODETYPE_IF = 65542,
	AST2C_NODETYPE_MEMBER_ACCESS = 65543,
	AST2C_NODETYPE_PAREN = 65544,
	AST2C_NODETYPE_INDEX = 65545,
	AST2C_NODETYPE_MATCH = 65546,
	AST2C_NODETYPE_RANGE = 65547,
	AST2C_NODETYPE_IDENTITY = 65548,
	AST2C_NODETYPE_TMP_VAR = 65549,
	AST2C_NODETYPE_IMPORT = 65550,
	AST2C_NODETYPE_CAST = 65551,
	AST2C_NODETYPE_AVAILABLE = 65552,
	AST2C_NODETYPE_SPECIFY_ORDER = 65553,
	AST2C_NODETYPE_EXPLICIT_ERROR = 65554,
	AST2C_NODETYPE_IO_OPERATION = 65555,
	AST2C_NODETYPE_OR_COND = 65556,
	AST2C_NODETYPE_BAD_EXPR = 65557,
	AST2C_NODETYPE_STMT = 131072,
	AST2C_NODETYPE_LOOP = 131073,
	AST2C_NODETYPE_INDENT_BLOCK = 131074,
	AST2C_NODETYPE_SCOPED_STATEMENT = 131075,
	AST2C_NODETYPE_MATCH_BRANCH = 131076,
	AST2C_NODETYPE_UNION_CANDIDATE = 131077,
	AST2C_NODETYPE_RETURN = 131078,
	AST2C_NODETYPE_BREAK = 131079,
	AST2C_NODETYPE_CONTINUE = 131080,
	AST2C_NODETYPE_ASSERT = 131081,
	AST2C_NODETYPE_IMPLICIT_YIELD = 131082,
	AST2C_NODETYPE_METADATA = 131083,
	AST2C_NODETYPE_TYPE = 262144,
	AST2C_NODETYPE_INT_TYPE = 262145,
	AST2C_NODETYPE_FLOAT_TYPE = 262146,
	AST2C_NODETYPE_IDENT_TYPE = 262147,
	AST2C_NODETYPE_INT_LITERAL_TYPE = 262148,
	AST2C_NODETYPE_STR_LITERAL_TYPE = 262149,
	AST2C_NODETYPE_REGEX_LITERAL_TYPE = 262150,
	AST2C_NODETYPE_VOID_TYPE = 262151,
	AST2C_NODETYPE_BOOL_TYPE = 262152,
	AST2C_NODETYPE_ARRAY_TYPE = 262153,
	AST2C_NODETYPE_FUNCTION_TYPE = 262154,
	AST2C_NODETYPE_STRUCT_TYPE = 262155,
	AST2C_NODETYPE_STRUCT_UNION_TYPE = 262156,
	AST2C_NODETYPE_UNION_TYPE = 262157,
	AST2C_NODETYPE_RANGE_TYPE = 262158,
	AST2C_NODETYPE_ENUM_TYPE = 262159,
	AST2C_NODETYPE_META_TYPE = 262160,
	AST2C_NODETYPE_OPTIONAL_TYPE = 262161,
	AST2C_NODETYPE_GENERIC_TYPE = 262162,
	AST2C_NODETYPE_LITERAL = 2162688,
	AST2C_NODETYPE_INT_LITERAL = 2162689,
	AST2C_NODETYPE_BOOL_LITERAL = 2162690,
	AST2C_NODETYPE_STR_LITERAL = 2162691,
	AST2C_NODETYPE_REGEX_LITERAL = 2162692,
	AST2C_NODETYPE_CHAR_LITERAL = 2162693,
	AST2C_NODETYPE_TYPE_LITERAL = 2162694,
	AST2C_NODETYPE_SPECIAL_LITERAL = 2162695,
	AST2C_NODETYPE_MEMBER = 2228224,
	AST2C_NODETYPE_FIELD = 2228225,
	AST2C_NODETYPE_FORMAT = 2228226,
	AST2C_NODETYPE_STATE = 2228227,
	AST2C_NODETYPE_ENUM = 2228228,
	AST2C_NODETYPE_ENUM_MEMBER = 2228229,
	AST2C_NODETYPE_FUNCTION = 2228230,
};
const char* ast2c_NodeType_to_string(ast2c_NodeType);
int ast2c_NodeType_from_string(const char*,ast2c_NodeType*);

enum ast2c_TokenTag {
	AST2C_TOKENTAG_INDENT = 0,
	AST2C_TOKENTAG_SPACE = 1,
	AST2C_TOKENTAG_LINE = 2,
	AST2C_TOKENTAG_PUNCT = 3,
	AST2C_TOKENTAG_INT_LITERAL = 4,
	AST2C_TOKENTAG_BOOL_LITERAL = 5,
	AST2C_TOKENTAG_STR_LITERAL = 6,
	AST2C_TOKENTAG_REGEX_LITERAL = 7,
	AST2C_TOKENTAG_CHAR_LITERAL = 8,
	AST2C_TOKENTAG_KEYWORD = 9,
	AST2C_TOKENTAG_IDENT = 10,
	AST2C_TOKENTAG_COMMENT = 11,
	AST2C_TOKENTAG_ERROR = 12,
	AST2C_TOKENTAG_UNKNOWN = 13,
};
const char* ast2c_TokenTag_to_string(ast2c_TokenTag);
int ast2c_TokenTag_from_string(const char*,ast2c_TokenTag*);

enum ast2c_UnaryOp {
	AST2C_UNARYOP_NOT = 0,
	AST2C_UNARYOP_MINUS_SIGN = 1,
};
const char* ast2c_UnaryOp_to_string(ast2c_UnaryOp);
int ast2c_UnaryOp_from_string(const char*,ast2c_UnaryOp*);

enum ast2c_BinaryOp {
	AST2C_BINARYOP_MUL = 0,
	AST2C_BINARYOP_DIV = 1,
	AST2C_BINARYOP_MOD = 2,
	AST2C_BINARYOP_LEFT_ARITHMETIC_SHIFT = 3,
	AST2C_BINARYOP_RIGHT_ARITHMETIC_SHIFT = 4,
	AST2C_BINARYOP_LEFT_LOGICAL_SHIFT = 5,
	AST2C_BINARYOP_RIGHT_LOGICAL_SHIFT = 6,
	AST2C_BINARYOP_BIT_AND = 7,
	AST2C_BINARYOP_ADD = 8,
	AST2C_BINARYOP_SUB = 9,
	AST2C_BINARYOP_BIT_OR = 10,
	AST2C_BINARYOP_BIT_XOR = 11,
	AST2C_BINARYOP_EQUAL = 12,
	AST2C_BINARYOP_NOT_EQUAL = 13,
	AST2C_BINARYOP_LESS = 14,
	AST2C_BINARYOP_LESS_OR_EQ = 15,
	AST2C_BINARYOP_GRATER = 16,
	AST2C_BINARYOP_GRATER_OR_EQ = 17,
	AST2C_BINARYOP_LOGICAL_AND = 18,
	AST2C_BINARYOP_LOGICAL_OR = 19,
	AST2C_BINARYOP_COND_OP_1 = 20,
	AST2C_BINARYOP_COND_OP_2 = 21,
	AST2C_BINARYOP_RANGE_EXCLUSIVE = 22,
	AST2C_BINARYOP_RANGE_INCLUSIVE = 23,
	AST2C_BINARYOP_ASSIGN = 24,
	AST2C_BINARYOP_DEFINE_ASSIGN = 25,
	AST2C_BINARYOP_CONST_ASSIGN = 26,
	AST2C_BINARYOP_ADD_ASSIGN = 27,
	AST2C_BINARYOP_SUB_ASSIGN = 28,
	AST2C_BINARYOP_MUL_ASSIGN = 29,
	AST2C_BINARYOP_DIV_ASSIGN = 30,
	AST2C_BINARYOP_MOD_ASSIGN = 31,
	AST2C_BINARYOP_LEFT_LOGICAL_SHIFT_ASSIGN = 32,
	AST2C_BINARYOP_RIGHT_LOGICAL_SHIFT_ASSIGN = 33,
	AST2C_BINARYOP_LEFT_ARITHMETIC_SHIFT_ASSIGN = 34,
	AST2C_BINARYOP_RIGHT_ARITHMETIC_SHIFT_ASSIGN = 35,
	AST2C_BINARYOP_BIT_AND_ASSIGN = 36,
	AST2C_BINARYOP_BIT_OR_ASSIGN = 37,
	AST2C_BINARYOP_BIT_XOR_ASSIGN = 38,
	AST2C_BINARYOP_COMMA = 39,
	AST2C_BINARYOP_IN_ASSIGN = 40,
};
const char* ast2c_BinaryOp_to_string(ast2c_BinaryOp);
int ast2c_BinaryOp_from_string(const char*,ast2c_BinaryOp*);

enum ast2c_IdentUsage {
	AST2C_IDENTUSAGE_UNKNOWN = 0,
	AST2C_IDENTUSAGE_BAD_IDENT = 1,
	AST2C_IDENTUSAGE_REFERENCE = 2,
	AST2C_IDENTUSAGE_DEFINE_VARIABLE = 3,
	AST2C_IDENTUSAGE_DEFINE_CONST = 4,
	AST2C_IDENTUSAGE_DEFINE_FIELD = 5,
	AST2C_IDENTUSAGE_DEFINE_FORMAT = 6,
	AST2C_IDENTUSAGE_DEFINE_STATE = 7,
	AST2C_IDENTUSAGE_DEFINE_ENUM = 8,
	AST2C_IDENTUSAGE_DEFINE_ENUM_MEMBER = 9,
	AST2C_IDENTUSAGE_DEFINE_FN = 10,
	AST2C_IDENTUSAGE_DEFINE_CAST_FN = 11,
	AST2C_IDENTUSAGE_DEFINE_ARG = 12,
	AST2C_IDENTUSAGE_REFERENCE_TYPE = 13,
	AST2C_IDENTUSAGE_REFERENCE_MEMBER = 14,
	AST2C_IDENTUSAGE_REFERENCE_MEMBER_TYPE = 15,
	AST2C_IDENTUSAGE_MAYBE_TYPE = 16,
	AST2C_IDENTUSAGE_REFERENCE_BUILTIN_FN = 17,
};
const char* ast2c_IdentUsage_to_string(ast2c_IdentUsage);
int ast2c_IdentUsage_from_string(const char*,ast2c_IdentUsage*);

enum ast2c_Endian {
	AST2C_ENDIAN_UNSPEC = 0,
	AST2C_ENDIAN_BIG = 1,
	AST2C_ENDIAN_LITTLE = 2,
};
const char* ast2c_Endian_to_string(ast2c_Endian);
int ast2c_Endian_from_string(const char*,ast2c_Endian*);

enum ast2c_ConstantLevel {
	AST2C_CONSTANTLEVEL_UNKNOWN = 0,
	AST2C_CONSTANTLEVEL_CONSTANT = 1,
	AST2C_CONSTANTLEVEL_IMMUTABLE_VARIABLE = 2,
	AST2C_CONSTANTLEVEL_VARIABLE = 3,
};
const char* ast2c_ConstantLevel_to_string(ast2c_ConstantLevel);
int ast2c_ConstantLevel_from_string(const char*,ast2c_ConstantLevel*);

enum ast2c_BitAlignment {
	AST2C_BITALIGNMENT_BYTE_ALIGNED = 0,
	AST2C_BITALIGNMENT_BIT_1 = 1,
	AST2C_BITALIGNMENT_BIT_2 = 2,
	AST2C_BITALIGNMENT_BIT_3 = 3,
	AST2C_BITALIGNMENT_BIT_4 = 4,
	AST2C_BITALIGNMENT_BIT_5 = 5,
	AST2C_BITALIGNMENT_BIT_6 = 6,
	AST2C_BITALIGNMENT_BIT_7 = 7,
	AST2C_BITALIGNMENT_NOT_TARGET = 8,
	AST2C_BITALIGNMENT_NOT_DECIDABLE = 9,
};
const char* ast2c_BitAlignment_to_string(ast2c_BitAlignment);
int ast2c_BitAlignment_from_string(const char*,ast2c_BitAlignment*);

enum ast2c_Follow {
	AST2C_FOLLOW_UNKNOWN = 0,
	AST2C_FOLLOW_END = 1,
	AST2C_FOLLOW_FIXED = 2,
	AST2C_FOLLOW_CONSTANT = 3,
	AST2C_FOLLOW_NORMAL = 4,
};
const char* ast2c_Follow_to_string(ast2c_Follow);
int ast2c_Follow_from_string(const char*,ast2c_Follow*);

enum ast2c_IoMethod {
	AST2C_IOMETHOD_UNSPEC = 0,
	AST2C_IOMETHOD_OUTPUT_PUT = 1,
	AST2C_IOMETHOD_INPUT_PEEK = 2,
	AST2C_IOMETHOD_INPUT_GET = 3,
	AST2C_IOMETHOD_INPUT_BACKWARD = 4,
	AST2C_IOMETHOD_INPUT_OFFSET = 5,
	AST2C_IOMETHOD_INPUT_BIT_OFFSET = 6,
	AST2C_IOMETHOD_INPUT_REMAIN = 7,
	AST2C_IOMETHOD_INPUT_SUBRANGE = 8,
	AST2C_IOMETHOD_CONFIG_ENDIAN_LITTLE = 9,
	AST2C_IOMETHOD_CONFIG_ENDIAN_BIG = 10,
	AST2C_IOMETHOD_CONFIG_ENDIAN_NATIVE = 11,
	AST2C_IOMETHOD_CONFIG_BIT_ORDER_LSB = 12,
	AST2C_IOMETHOD_CONFIG_BIT_ORDER_MSB = 13,
};
const char* ast2c_IoMethod_to_string(ast2c_IoMethod);
int ast2c_IoMethod_from_string(const char*,ast2c_IoMethod*);

enum ast2c_SpecialLiteralKind {
	AST2C_SPECIALLITERALKIND_INPUT = 0,
	AST2C_SPECIALLITERALKIND_OUTPUT = 1,
	AST2C_SPECIALLITERALKIND_CONFIG = 2,
};
const char* ast2c_SpecialLiteralKind_to_string(ast2c_SpecialLiteralKind);
int ast2c_SpecialLiteralKind_from_string(const char*,ast2c_SpecialLiteralKind*);

enum ast2c_OrderType {
	AST2C_ORDERTYPE_BYTE = 0,
	AST2C_ORDERTYPE_BIT_STREAM = 1,
	AST2C_ORDERTYPE_BIT_MAPPING = 2,
	AST2C_ORDERTYPE_BIT_BOTH = 3,
};
const char* ast2c_OrderType_to_string(ast2c_OrderType);
int ast2c_OrderType_from_string(const char*,ast2c_OrderType*);

enum ast2c_FormatTrait {
	AST2C_FORMATTRAIT_NONE = 0,
	AST2C_FORMATTRAIT_FIXED_PRIMITIVE = 1,
	AST2C_FORMATTRAIT_FIXED_FLOAT = 2,
	AST2C_FORMATTRAIT_FIXED_ARRAY = 4,
	AST2C_FORMATTRAIT_VARIABLE_ARRAY = 8,
	AST2C_FORMATTRAIT_STRUCT = 16,
	AST2C_FORMATTRAIT_CONDITIONAL = 32,
	AST2C_FORMATTRAIT_STATIC_PEEK = 64,
	AST2C_FORMATTRAIT_BIT_FIELD = 128,
	AST2C_FORMATTRAIT_READ_STATE = 256,
	AST2C_FORMATTRAIT_WRITE_STATE = 512,
	AST2C_FORMATTRAIT_TERMINAL_PATTERN = 1024,
	AST2C_FORMATTRAIT_BIT_STREAM = 2048,
	AST2C_FORMATTRAIT_DYNAMIC_ORDER = 4096,
	AST2C_FORMATTRAIT_FULL_INPUT = 8192,
	AST2C_FORMATTRAIT_BACKWARD_INPUT = 16384,
	AST2C_FORMATTRAIT_MAGIC_VALUE = 32768,
	AST2C_FORMATTRAIT_ASSERTION = 65536,
	AST2C_FORMATTRAIT_EXPLICIT_ERROR = 131072,
	AST2C_FORMATTRAIT_PROCEDURAL = 262144,
	AST2C_FORMATTRAIT_FOR_LOOP = 524288,
	AST2C_FORMATTRAIT_LOCAL_VARIABLE = 1048576,
	AST2C_FORMATTRAIT_DESCRIPTION_ONLY = 2097152,
	AST2C_FORMATTRAIT_UNCOMMON_SIZE = 4194304,
};
const char* ast2c_FormatTrait_to_string(ast2c_FormatTrait);
int ast2c_FormatTrait_from_string(const char*,ast2c_FormatTrait*);

struct ast2c_Pos {
	uint64_t begin;
	uint64_t end;
};

// returns 1 if succeed 0 if failed
int ast2c_Pos_parse(ast2c_Pos*,ast2c_json_handlers*,void*);

struct ast2c_Loc {
	ast2c_Pos pos;
	uint64_t file;
	uint64_t line;
	uint64_t col;
};

// returns 1 if succeed 0 if failed
int ast2c_Loc_parse(ast2c_Loc*,ast2c_json_handlers*,void*);

struct ast2c_Token {
	ast2c_TokenTag tag;
	char* token;
	ast2c_Loc loc;
};

struct ast2c_RawScope {
	void* prev;
	void* next;
	void* branch;
	void* ident;
	size_t ident_size;
	void* owner;
	int branch_root;
};

struct ast2c_RawNode {
	ast2c_NodeType node_type;
	ast2c_Loc loc;
	void* body;
};

struct ast2c_SrcErrorEntry {
	char* msg;
	char* file;
	ast2c_Loc loc;
	char* src;
	int warn;
};

struct ast2c_SrcError {
	ast2c_SrcErrorEntry* errs;
	size_t errs_size;
};

struct ast2c_JsonAst {
	ast2c_RawNode* node;
	size_t node_size;
	ast2c_RawScope* scope;
	size_t scope_size;
};

struct ast2c_AstFile {
	int success;
	char** files;
	size_t files_size;
	ast2c_JsonAst* ast;
	ast2c_SrcError* error;
};

struct ast2c_TokenFile {
	int success;
	char** files;
	size_t files_size;
	ast2c_Token* tokens;
	size_t tokens_size;
	ast2c_SrcError* error;
};

struct ast2c_Node {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
};

struct ast2c_Expr {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
};

struct ast2c_Stmt {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
};

struct ast2c_Type {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
};

struct ast2c_Literal {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
};

struct ast2c_Member {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
};

struct ast2c_Program {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_StructType* struct_type;
	ast2c_Node** elements;
	size_t elements_size;
	ast2c_Scope* global_scope;
	ast2c_Metadata** metadata;
	size_t metadata_size;
};

// returns 1 if succeed 0 if failed
int ast2c_Program_parse(ast2c_Ast* ,ast2c_Program*,ast2c_json_handlers*,void*);

struct ast2c_Comment {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	char* comment;
};

// returns 1 if succeed 0 if failed
int ast2c_Comment_parse(ast2c_Ast* ,ast2c_Comment*,ast2c_json_handlers*,void*);

struct ast2c_CommentGroup {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Comment** comments;
	size_t comments_size;
};

// returns 1 if succeed 0 if failed
int ast2c_CommentGroup_parse(ast2c_Ast* ,ast2c_CommentGroup*,ast2c_json_handlers*,void*);

struct ast2c_FieldArgument {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Expr* raw_arguments;
	ast2c_Loc end_loc;
	ast2c_Expr** collected_arguments;
	size_t collected_arguments_size;
	ast2c_Expr** arguments;
	size_t arguments_size;
	ast2c_Expr* alignment;
	uint64_t* alignment_value;
	ast2c_Expr* sub_byte_length;
	ast2c_Expr* sub_byte_begin;
	ast2c_Expr* peek;
	uint64_t* peek_value;
	ast2c_TypeLiteral* type_map;
	ast2c_Metadata** metadata;
	size_t metadata_size;
};

// returns 1 if succeed 0 if failed
int ast2c_FieldArgument_parse(ast2c_Ast* ,ast2c_FieldArgument*,ast2c_json_handlers*,void*);

struct ast2c_Binary {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_BinaryOp op;
	ast2c_Expr* left;
	ast2c_Expr* right;
};

// returns 1 if succeed 0 if failed
int ast2c_Binary_parse(ast2c_Ast* ,ast2c_Binary*,ast2c_json_handlers*,void*);

struct ast2c_Unary {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_UnaryOp op;
	ast2c_Expr* expr;
};

// returns 1 if succeed 0 if failed
int ast2c_Unary_parse(ast2c_Ast* ,ast2c_Unary*,ast2c_json_handlers*,void*);

struct ast2c_Cond {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* cond;
	ast2c_Expr* then;
	ast2c_Loc els_loc;
	ast2c_Expr* els;
};

// returns 1 if succeed 0 if failed
int ast2c_Cond_parse(ast2c_Ast* ,ast2c_Cond*,ast2c_json_handlers*,void*);

struct ast2c_Ident {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* ident;
	ast2c_IdentUsage usage;
	ast2c_Node* base;
	ast2c_Scope* scope;
};

// returns 1 if succeed 0 if failed
int ast2c_Ident_parse(ast2c_Ast* ,ast2c_Ident*,ast2c_json_handlers*,void*);

struct ast2c_Call {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* callee;
	ast2c_Expr* raw_arguments;
	ast2c_Expr** arguments;
	size_t arguments_size;
	ast2c_Loc end_loc;
};

// returns 1 if succeed 0 if failed
int ast2c_Call_parse(ast2c_Ast* ,ast2c_Call*,ast2c_json_handlers*,void*);

struct ast2c_If {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_StructUnionType* struct_union_type;
	ast2c_Scope* cond_scope;
	ast2c_Identity* cond;
	ast2c_IndentBlock* then;
	ast2c_Node* els;
};

// returns 1 if succeed 0 if failed
int ast2c_If_parse(ast2c_Ast* ,ast2c_If*,ast2c_json_handlers*,void*);

struct ast2c_MemberAccess {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* target;
	ast2c_Ident* member;
	ast2c_Ident* base;
};

// returns 1 if succeed 0 if failed
int ast2c_MemberAccess_parse(ast2c_Ast* ,ast2c_MemberAccess*,ast2c_json_handlers*,void*);

struct ast2c_Paren {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* expr;
	ast2c_Loc end_loc;
};

// returns 1 if succeed 0 if failed
int ast2c_Paren_parse(ast2c_Ast* ,ast2c_Paren*,ast2c_json_handlers*,void*);

struct ast2c_Index {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* expr;
	ast2c_Expr* index;
	ast2c_Loc end_loc;
};

// returns 1 if succeed 0 if failed
int ast2c_Index_parse(ast2c_Ast* ,ast2c_Index*,ast2c_json_handlers*,void*);

struct ast2c_Match {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_StructUnionType* struct_union_type;
	ast2c_Scope* cond_scope;
	ast2c_Identity* cond;
	ast2c_MatchBranch** branch;
	size_t branch_size;
};

// returns 1 if succeed 0 if failed
int ast2c_Match_parse(ast2c_Ast* ,ast2c_Match*,ast2c_json_handlers*,void*);

struct ast2c_Range {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_BinaryOp op;
	ast2c_Expr* start;
	ast2c_Expr* end;
};

// returns 1 if succeed 0 if failed
int ast2c_Range_parse(ast2c_Ast* ,ast2c_Range*,ast2c_json_handlers*,void*);

struct ast2c_Identity {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* expr;
};

// returns 1 if succeed 0 if failed
int ast2c_Identity_parse(ast2c_Ast* ,ast2c_Identity*,ast2c_json_handlers*,void*);

struct ast2c_TmpVar {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	uint64_t tmp_var;
};

// returns 1 if succeed 0 if failed
int ast2c_TmpVar_parse(ast2c_Ast* ,ast2c_TmpVar*,ast2c_json_handlers*,void*);

struct ast2c_Import {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* path;
	ast2c_Call* base;
	ast2c_Program* import_desc;
};

// returns 1 if succeed 0 if failed
int ast2c_Import_parse(ast2c_Ast* ,ast2c_Import*,ast2c_json_handlers*,void*);

struct ast2c_Cast {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Call* base;
	ast2c_Expr* expr;
};

// returns 1 if succeed 0 if failed
int ast2c_Cast_parse(ast2c_Ast* ,ast2c_Cast*,ast2c_json_handlers*,void*);

struct ast2c_Available {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Call* base;
	ast2c_Expr* target;
};

// returns 1 if succeed 0 if failed
int ast2c_Available_parse(ast2c_Ast* ,ast2c_Available*,ast2c_json_handlers*,void*);

struct ast2c_SpecifyOrder {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Binary* base;
	ast2c_OrderType order_type;
	ast2c_Expr* order;
	uint64_t* order_value;
};

// returns 1 if succeed 0 if failed
int ast2c_SpecifyOrder_parse(ast2c_Ast* ,ast2c_SpecifyOrder*,ast2c_json_handlers*,void*);

struct ast2c_ExplicitError {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Call* base;
	ast2c_StrLiteral* message;
};

// returns 1 if succeed 0 if failed
int ast2c_ExplicitError_parse(ast2c_Ast* ,ast2c_ExplicitError*,ast2c_json_handlers*,void*);

struct ast2c_IoOperation {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Expr* base;
	ast2c_IoMethod method;
	ast2c_Expr** arguments;
	size_t arguments_size;
};

// returns 1 if succeed 0 if failed
int ast2c_IoOperation_parse(ast2c_Ast* ,ast2c_IoOperation*,ast2c_json_handlers*,void*);

struct ast2c_OrCond {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Binary* base;
	ast2c_Expr** conds;
	size_t conds_size;
};

// returns 1 if succeed 0 if failed
int ast2c_OrCond_parse(ast2c_Ast* ,ast2c_OrCond*,ast2c_json_handlers*,void*);

struct ast2c_BadExpr {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* content;
	ast2c_Expr* bad_expr;
};

// returns 1 if succeed 0 if failed
int ast2c_BadExpr_parse(ast2c_Ast* ,ast2c_BadExpr*,ast2c_json_handlers*,void*);

struct ast2c_Loop {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Scope* cond_scope;
	ast2c_Expr* init;
	ast2c_Expr* cond;
	ast2c_Expr* step;
	ast2c_IndentBlock* body;
};

// returns 1 if succeed 0 if failed
int ast2c_Loop_parse(ast2c_Ast* ,ast2c_Loop*,ast2c_json_handlers*,void*);

struct ast2c_IndentBlock {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_StructType* struct_type;
	ast2c_Node** elements;
	size_t elements_size;
	ast2c_Scope* scope;
	ast2c_Metadata** metadata;
	size_t metadata_size;
};

// returns 1 if succeed 0 if failed
int ast2c_IndentBlock_parse(ast2c_Ast* ,ast2c_IndentBlock*,ast2c_json_handlers*,void*);

struct ast2c_ScopedStatement {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_StructType* struct_type;
	ast2c_Node* statement;
	ast2c_Scope* scope;
};

// returns 1 if succeed 0 if failed
int ast2c_ScopedStatement_parse(ast2c_Ast* ,ast2c_ScopedStatement*,ast2c_json_handlers*,void*);

struct ast2c_MatchBranch {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Match* belong;
	ast2c_Identity* cond;
	ast2c_Loc sym_loc;
	ast2c_Node* then;
};

// returns 1 if succeed 0 if failed
int ast2c_MatchBranch_parse(ast2c_Ast* ,ast2c_MatchBranch*,ast2c_json_handlers*,void*);

struct ast2c_UnionCandidate {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Expr* cond;
	ast2c_Field* field;
};

// returns 1 if succeed 0 if failed
int ast2c_UnionCandidate_parse(ast2c_Ast* ,ast2c_UnionCandidate*,ast2c_json_handlers*,void*);

struct ast2c_Return {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Expr* expr;
	ast2c_Function* related_function;
};

// returns 1 if succeed 0 if failed
int ast2c_Return_parse(ast2c_Ast* ,ast2c_Return*,ast2c_json_handlers*,void*);

struct ast2c_Break {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Loop* related_loop;
};

// returns 1 if succeed 0 if failed
int ast2c_Break_parse(ast2c_Ast* ,ast2c_Break*,ast2c_json_handlers*,void*);

struct ast2c_Continue {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Loop* related_loop;
};

// returns 1 if succeed 0 if failed
int ast2c_Continue_parse(ast2c_Ast* ,ast2c_Continue*,ast2c_json_handlers*,void*);

struct ast2c_Assert {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Binary* cond;
	int is_io_related;
};

// returns 1 if succeed 0 if failed
int ast2c_Assert_parse(ast2c_Ast* ,ast2c_Assert*,ast2c_json_handlers*,void*);

struct ast2c_ImplicitYield {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Expr* expr;
};

// returns 1 if succeed 0 if failed
int ast2c_ImplicitYield_parse(ast2c_Ast* ,ast2c_ImplicitYield*,ast2c_json_handlers*,void*);

struct ast2c_Metadata {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Expr* base;
	char* name;
	ast2c_Expr** values;
	size_t values_size;
};

// returns 1 if succeed 0 if failed
int ast2c_Metadata_parse(ast2c_Ast* ,ast2c_Metadata*,ast2c_json_handlers*,void*);

struct ast2c_IntType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Endian endian;
	int is_signed;
	int is_common_supported;
};

// returns 1 if succeed 0 if failed
int ast2c_IntType_parse(ast2c_Ast* ,ast2c_IntType*,ast2c_json_handlers*,void*);

struct ast2c_FloatType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Endian endian;
	int is_common_supported;
};

// returns 1 if succeed 0 if failed
int ast2c_FloatType_parse(ast2c_Ast* ,ast2c_FloatType*,ast2c_json_handlers*,void*);

struct ast2c_IdentType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_MemberAccess* import_ref;
	ast2c_Ident* ident;
	ast2c_Type* base;
};

// returns 1 if succeed 0 if failed
int ast2c_IdentType_parse(ast2c_Ast* ,ast2c_IdentType*,ast2c_json_handlers*,void*);

struct ast2c_IntLiteralType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_IntLiteral* base;
};

// returns 1 if succeed 0 if failed
int ast2c_IntLiteralType_parse(ast2c_Ast* ,ast2c_IntLiteralType*,ast2c_json_handlers*,void*);

struct ast2c_StrLiteralType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_StrLiteral* base;
	ast2c_StrLiteral* strong_ref;
};

// returns 1 if succeed 0 if failed
int ast2c_StrLiteralType_parse(ast2c_Ast* ,ast2c_StrLiteralType*,ast2c_json_handlers*,void*);

struct ast2c_RegexLiteralType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_RegexLiteral* base;
	ast2c_RegexLiteral* strong_ref;
};

// returns 1 if succeed 0 if failed
int ast2c_RegexLiteralType_parse(ast2c_Ast* ,ast2c_RegexLiteralType*,ast2c_json_handlers*,void*);

struct ast2c_VoidType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
};

// returns 1 if succeed 0 if failed
int ast2c_VoidType_parse(ast2c_Ast* ,ast2c_VoidType*,ast2c_json_handlers*,void*);

struct ast2c_BoolType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
};

// returns 1 if succeed 0 if failed
int ast2c_BoolType_parse(ast2c_Ast* ,ast2c_BoolType*,ast2c_json_handlers*,void*);

struct ast2c_ArrayType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Loc end_loc;
	ast2c_Type* element_type;
	ast2c_Expr* length;
	uint64_t* length_value;
	int is_bytes;
};

// returns 1 if succeed 0 if failed
int ast2c_ArrayType_parse(ast2c_Ast* ,ast2c_ArrayType*,ast2c_json_handlers*,void*);

struct ast2c_FunctionType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Type* return_type;
	ast2c_Type** parameters;
	size_t parameters_size;
};

// returns 1 if succeed 0 if failed
int ast2c_FunctionType_parse(ast2c_Ast* ,ast2c_FunctionType*,ast2c_json_handlers*,void*);

struct ast2c_StructType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Member** fields;
	size_t fields_size;
	ast2c_Node* base;
	int recursive;
	uint64_t fixed_header_size;
	uint64_t fixed_tail_size;
};

// returns 1 if succeed 0 if failed
int ast2c_StructType_parse(ast2c_Ast* ,ast2c_StructType*,ast2c_json_handlers*,void*);

struct ast2c_StructUnionType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Expr* cond;
	ast2c_Expr** conds;
	size_t conds_size;
	ast2c_StructType** structs;
	size_t structs_size;
	ast2c_Expr* base;
	ast2c_Field** union_fields;
	size_t union_fields_size;
	int exhaustive;
};

// returns 1 if succeed 0 if failed
int ast2c_StructUnionType_parse(ast2c_Ast* ,ast2c_StructUnionType*,ast2c_json_handlers*,void*);

struct ast2c_UnionType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Expr* cond;
	ast2c_UnionCandidate** candidates;
	size_t candidates_size;
	ast2c_StructUnionType* base_type;
	ast2c_Type* common_type;
	ast2c_Field** member_candidates;
	size_t member_candidates_size;
};

// returns 1 if succeed 0 if failed
int ast2c_UnionType_parse(ast2c_Ast* ,ast2c_UnionType*,ast2c_json_handlers*,void*);

struct ast2c_RangeType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Type* base_type;
	ast2c_Range* range;
};

// returns 1 if succeed 0 if failed
int ast2c_RangeType_parse(ast2c_Ast* ,ast2c_RangeType*,ast2c_json_handlers*,void*);

struct ast2c_EnumType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Enum* base;
};

// returns 1 if succeed 0 if failed
int ast2c_EnumType_parse(ast2c_Ast* ,ast2c_EnumType*,ast2c_json_handlers*,void*);

struct ast2c_MetaType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
};

// returns 1 if succeed 0 if failed
int ast2c_MetaType_parse(ast2c_Ast* ,ast2c_MetaType*,ast2c_json_handlers*,void*);

struct ast2c_OptionalType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Type* base_type;
};

// returns 1 if succeed 0 if failed
int ast2c_OptionalType_parse(ast2c_Ast* ,ast2c_OptionalType*,ast2c_json_handlers*,void*);

struct ast2c_GenericType {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	int is_explicit;
	int non_dynamic_allocation;
	ast2c_BitAlignment bit_alignment;
	uint64_t* bit_size;
	ast2c_Member* belong;
};

// returns 1 if succeed 0 if failed
int ast2c_GenericType_parse(ast2c_Ast* ,ast2c_GenericType*,ast2c_json_handlers*,void*);

struct ast2c_IntLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* value;
};

// returns 1 if succeed 0 if failed
int ast2c_IntLiteral_parse(ast2c_Ast* ,ast2c_IntLiteral*,ast2c_json_handlers*,void*);

struct ast2c_BoolLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	int value;
};

// returns 1 if succeed 0 if failed
int ast2c_BoolLiteral_parse(ast2c_Ast* ,ast2c_BoolLiteral*,ast2c_json_handlers*,void*);

struct ast2c_StrLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* value;
	uint64_t length;
};

// returns 1 if succeed 0 if failed
int ast2c_StrLiteral_parse(ast2c_Ast* ,ast2c_StrLiteral*,ast2c_json_handlers*,void*);

struct ast2c_RegexLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* value;
};

// returns 1 if succeed 0 if failed
int ast2c_RegexLiteral_parse(ast2c_Ast* ,ast2c_RegexLiteral*,ast2c_json_handlers*,void*);

struct ast2c_CharLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* value;
	uint64_t code;
};

// returns 1 if succeed 0 if failed
int ast2c_CharLiteral_parse(ast2c_Ast* ,ast2c_CharLiteral*,ast2c_json_handlers*,void*);

struct ast2c_TypeLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_Type* type_literal;
	ast2c_Loc end_loc;
};

// returns 1 if succeed 0 if failed
int ast2c_TypeLiteral_parse(ast2c_Ast* ,ast2c_TypeLiteral*,ast2c_json_handlers*,void*);

struct ast2c_SpecialLiteral {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	ast2c_SpecialLiteralKind kind;
};

// returns 1 if succeed 0 if failed
int ast2c_SpecialLiteral_parse(ast2c_Ast* ,ast2c_SpecialLiteral*,ast2c_json_handlers*,void*);

struct ast2c_Field {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_Loc colon_loc;
	int is_state_variable;
	ast2c_Type* field_type;
	ast2c_FieldArgument* arguments;
	uint64_t* offset_bit;
	uint64_t offset_recent;
	uint64_t* tail_offset_bit;
	uint64_t tail_offset_recent;
	ast2c_BitAlignment bit_alignment;
	ast2c_BitAlignment eventual_bit_alignment;
	ast2c_Follow follow;
	ast2c_Follow eventual_follow;
	ast2c_Field* next;
};

// returns 1 if succeed 0 if failed
int ast2c_Field_parse(ast2c_Ast* ,ast2c_Field*,ast2c_json_handlers*,void*);

struct ast2c_Format {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_IndentBlock* body;
	ast2c_Function* encode_fn;
	ast2c_Function* decode_fn;
	ast2c_Function** cast_fns;
	size_t cast_fns_size;
	ast2c_IdentType** depends;
	size_t depends_size;
	ast2c_Field** state_variables;
	size_t state_variables_size;
	ast2c_FormatTrait format_trait;
};

// returns 1 if succeed 0 if failed
int ast2c_Format_parse(ast2c_Ast* ,ast2c_Format*,ast2c_json_handlers*,void*);

struct ast2c_State {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_IndentBlock* body;
};

// returns 1 if succeed 0 if failed
int ast2c_State_parse(ast2c_Ast* ,ast2c_State*,ast2c_json_handlers*,void*);

struct ast2c_Enum {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_Scope* scope;
	ast2c_Loc colon_loc;
	ast2c_Type* base_type;
	ast2c_EnumMember** members;
	size_t members_size;
	ast2c_EnumType* enum_type;
};

// returns 1 if succeed 0 if failed
int ast2c_Enum_parse(ast2c_Ast* ,ast2c_Enum*,ast2c_json_handlers*,void*);

struct ast2c_EnumMember {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_Expr* raw_expr;
	ast2c_Expr* value;
	ast2c_StrLiteral* str_literal;
};

// returns 1 if succeed 0 if failed
int ast2c_EnumMember_parse(ast2c_Ast* ,ast2c_EnumMember*,ast2c_json_handlers*,void*);

struct ast2c_Function {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_Field** parameters;
	size_t parameters_size;
	ast2c_Type* return_type;
	ast2c_IndentBlock* body;
	ast2c_FunctionType* func_type;
	int is_cast;
};

// returns 1 if succeed 0 if failed
int ast2c_Function_parse(ast2c_Ast* ,ast2c_Function*,ast2c_json_handlers*,void*);

struct ast2c_Scope {
	const ast2c_NodeType node_type;
	ast2c_Scope* prev;
	ast2c_Scope* next;
	ast2c_Scope* branch;
	ast2c_Ident** ident;
	size_t ident_size;
	ast2c_Node* owner;
	int branch_root;
};

// returns 1 if succeed 0 if failed
int ast2c_Scope_parse(ast2c_Ast* ,ast2c_Scope*,ast2c_json_handlers*,void*);

struct ast2c_Ast {
	ast2c_Node** nodes;
	size_t nodes_size;
	ast2c_Scope** scopes;
	size_t scopes_size;
};

#ifdef __cplusplus
}
#endif

#endif