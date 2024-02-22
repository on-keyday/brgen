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
typedef struct ast2c_Node ast2c_Node;
typedef struct ast2c_Expr ast2c_Expr;
typedef struct ast2c_Stmt ast2c_Stmt;
typedef struct ast2c_Type ast2c_Type;
typedef struct ast2c_Literal ast2c_Literal;
typedef struct ast2c_Member ast2c_Member;
typedef struct ast2c_BuiltinMember ast2c_BuiltinMember;
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
typedef struct ast2c_TmpVar ast2c_TmpVar;
typedef struct ast2c_Import ast2c_Import;
typedef struct ast2c_Cast ast2c_Cast;
typedef struct ast2c_Available ast2c_Available;
typedef struct ast2c_SpecifyOrder ast2c_SpecifyOrder;
typedef struct ast2c_ExplicitError ast2c_ExplicitError;
typedef struct ast2c_IoOperation ast2c_IoOperation;
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
typedef struct ast2c_CharLiteral ast2c_CharLiteral;
typedef struct ast2c_TypeLiteral ast2c_TypeLiteral;
typedef struct ast2c_SpecialLiteral ast2c_SpecialLiteral;
typedef struct ast2c_Field ast2c_Field;
typedef struct ast2c_Format ast2c_Format;
typedef struct ast2c_State ast2c_State;
typedef struct ast2c_Enum ast2c_Enum;
typedef struct ast2c_EnumMember ast2c_EnumMember;
typedef struct ast2c_Function ast2c_Function;
typedef struct ast2c_BuiltinFunction ast2c_BuiltinFunction;
typedef struct ast2c_BuiltinField ast2c_BuiltinField;
typedef struct ast2c_BuiltinObject ast2c_BuiltinObject;
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
	AST2C_NODETYPE_PROGRAM,
	AST2C_NODETYPE_COMMENT,
	AST2C_NODETYPE_COMMENT_GROUP,
	AST2C_NODETYPE_FIELD_ARGUMENT,
	AST2C_NODETYPE_EXPR,
	AST2C_NODETYPE_BINARY,
	AST2C_NODETYPE_UNARY,
	AST2C_NODETYPE_COND,
	AST2C_NODETYPE_IDENT,
	AST2C_NODETYPE_CALL,
	AST2C_NODETYPE_IF,
	AST2C_NODETYPE_MEMBER_ACCESS,
	AST2C_NODETYPE_PAREN,
	AST2C_NODETYPE_INDEX,
	AST2C_NODETYPE_MATCH,
	AST2C_NODETYPE_RANGE,
	AST2C_NODETYPE_TMP_VAR,
	AST2C_NODETYPE_IMPORT,
	AST2C_NODETYPE_CAST,
	AST2C_NODETYPE_AVAILABLE,
	AST2C_NODETYPE_SPECIFY_ORDER,
	AST2C_NODETYPE_EXPLICIT_ERROR,
	AST2C_NODETYPE_IO_OPERATION,
	AST2C_NODETYPE_BAD_EXPR,
	AST2C_NODETYPE_STMT,
	AST2C_NODETYPE_LOOP,
	AST2C_NODETYPE_INDENT_BLOCK,
	AST2C_NODETYPE_SCOPED_STATEMENT,
	AST2C_NODETYPE_MATCH_BRANCH,
	AST2C_NODETYPE_UNION_CANDIDATE,
	AST2C_NODETYPE_RETURN,
	AST2C_NODETYPE_BREAK,
	AST2C_NODETYPE_CONTINUE,
	AST2C_NODETYPE_ASSERT,
	AST2C_NODETYPE_IMPLICIT_YIELD,
	AST2C_NODETYPE_METADATA,
	AST2C_NODETYPE_TYPE,
	AST2C_NODETYPE_INT_TYPE,
	AST2C_NODETYPE_FLOAT_TYPE,
	AST2C_NODETYPE_IDENT_TYPE,
	AST2C_NODETYPE_INT_LITERAL_TYPE,
	AST2C_NODETYPE_STR_LITERAL_TYPE,
	AST2C_NODETYPE_VOID_TYPE,
	AST2C_NODETYPE_BOOL_TYPE,
	AST2C_NODETYPE_ARRAY_TYPE,
	AST2C_NODETYPE_FUNCTION_TYPE,
	AST2C_NODETYPE_STRUCT_TYPE,
	AST2C_NODETYPE_STRUCT_UNION_TYPE,
	AST2C_NODETYPE_UNION_TYPE,
	AST2C_NODETYPE_RANGE_TYPE,
	AST2C_NODETYPE_ENUM_TYPE,
	AST2C_NODETYPE_META_TYPE,
	AST2C_NODETYPE_OPTIONAL_TYPE,
	AST2C_NODETYPE_GENERIC_TYPE,
	AST2C_NODETYPE_LITERAL,
	AST2C_NODETYPE_INT_LITERAL,
	AST2C_NODETYPE_BOOL_LITERAL,
	AST2C_NODETYPE_STR_LITERAL,
	AST2C_NODETYPE_CHAR_LITERAL,
	AST2C_NODETYPE_TYPE_LITERAL,
	AST2C_NODETYPE_SPECIAL_LITERAL,
	AST2C_NODETYPE_MEMBER,
	AST2C_NODETYPE_FIELD,
	AST2C_NODETYPE_FORMAT,
	AST2C_NODETYPE_STATE,
	AST2C_NODETYPE_ENUM,
	AST2C_NODETYPE_ENUM_MEMBER,
	AST2C_NODETYPE_FUNCTION,
	AST2C_NODETYPE_BUILTIN_MEMBER,
	AST2C_NODETYPE_BUILTIN_FUNCTION,
	AST2C_NODETYPE_BUILTIN_FIELD,
	AST2C_NODETYPE_BUILTIN_OBJECT,
};
const char* ast2c_NodeType_to_string(ast2c_NodeType);
int ast2c_NodeType_from_string(const char*,ast2c_NodeType*);

enum ast2c_TokenTag {
	AST2C_TOKENTAG_INDENT,
	AST2C_TOKENTAG_SPACE,
	AST2C_TOKENTAG_LINE,
	AST2C_TOKENTAG_PUNCT,
	AST2C_TOKENTAG_INT_LITERAL,
	AST2C_TOKENTAG_BOOL_LITERAL,
	AST2C_TOKENTAG_STR_LITERAL,
	AST2C_TOKENTAG_CHAR_LITERAL,
	AST2C_TOKENTAG_KEYWORD,
	AST2C_TOKENTAG_IDENT,
	AST2C_TOKENTAG_COMMENT,
	AST2C_TOKENTAG_ERROR,
	AST2C_TOKENTAG_UNKNOWN,
};
const char* ast2c_TokenTag_to_string(ast2c_TokenTag);
int ast2c_TokenTag_from_string(const char*,ast2c_TokenTag*);

enum ast2c_UnaryOp {
	AST2C_UNARYOP_NOT,
	AST2C_UNARYOP_MINUS_SIGN,
};
const char* ast2c_UnaryOp_to_string(ast2c_UnaryOp);
int ast2c_UnaryOp_from_string(const char*,ast2c_UnaryOp*);

enum ast2c_BinaryOp {
	AST2C_BINARYOP_MUL,
	AST2C_BINARYOP_DIV,
	AST2C_BINARYOP_MOD,
	AST2C_BINARYOP_LEFT_ARITHMETIC_SHIFT,
	AST2C_BINARYOP_RIGHT_ARITHMETIC_SHIFT,
	AST2C_BINARYOP_LEFT_LOGICAL_SHIFT,
	AST2C_BINARYOP_RIGHT_LOGICAL_SHIFT,
	AST2C_BINARYOP_BIT_AND,
	AST2C_BINARYOP_ADD,
	AST2C_BINARYOP_SUB,
	AST2C_BINARYOP_BIT_OR,
	AST2C_BINARYOP_BIT_XOR,
	AST2C_BINARYOP_EQUAL,
	AST2C_BINARYOP_NOT_EQUAL,
	AST2C_BINARYOP_LESS,
	AST2C_BINARYOP_LESS_OR_EQ,
	AST2C_BINARYOP_GRATER,
	AST2C_BINARYOP_GRATER_OR_EQ,
	AST2C_BINARYOP_LOGICAL_AND,
	AST2C_BINARYOP_LOGICAL_OR,
	AST2C_BINARYOP_COND_OP_1,
	AST2C_BINARYOP_COND_OP_2,
	AST2C_BINARYOP_RANGE_EXCLUSIVE,
	AST2C_BINARYOP_RANGE_INCLUSIVE,
	AST2C_BINARYOP_ASSIGN,
	AST2C_BINARYOP_DEFINE_ASSIGN,
	AST2C_BINARYOP_CONST_ASSIGN,
	AST2C_BINARYOP_ADD_ASSIGN,
	AST2C_BINARYOP_SUB_ASSIGN,
	AST2C_BINARYOP_MUL_ASSIGN,
	AST2C_BINARYOP_DIV_ASSIGN,
	AST2C_BINARYOP_MOD_ASSIGN,
	AST2C_BINARYOP_LEFT_SHIFT_ASSIGN,
	AST2C_BINARYOP_RIGHT_SHIFT_ASSIGN,
	AST2C_BINARYOP_BIT_AND_ASSIGN,
	AST2C_BINARYOP_BIT_OR_ASSIGN,
	AST2C_BINARYOP_BIT_XOR_ASSIGN,
	AST2C_BINARYOP_COMMA,
};
const char* ast2c_BinaryOp_to_string(ast2c_BinaryOp);
int ast2c_BinaryOp_from_string(const char*,ast2c_BinaryOp*);

enum ast2c_IdentUsage {
	AST2C_IDENTUSAGE_UNKNOWN,
	AST2C_IDENTUSAGE_REFERENCE,
	AST2C_IDENTUSAGE_DEFINE_VARIABLE,
	AST2C_IDENTUSAGE_DEFINE_CONST,
	AST2C_IDENTUSAGE_DEFINE_FIELD,
	AST2C_IDENTUSAGE_DEFINE_FORMAT,
	AST2C_IDENTUSAGE_DEFINE_STATE,
	AST2C_IDENTUSAGE_DEFINE_ENUM,
	AST2C_IDENTUSAGE_DEFINE_ENUM_MEMBER,
	AST2C_IDENTUSAGE_DEFINE_FN,
	AST2C_IDENTUSAGE_DEFINE_CAST_FN,
	AST2C_IDENTUSAGE_DEFINE_ARG,
	AST2C_IDENTUSAGE_REFERENCE_TYPE,
	AST2C_IDENTUSAGE_REFERENCE_MEMBER,
	AST2C_IDENTUSAGE_REFERENCE_MEMBER_TYPE,
	AST2C_IDENTUSAGE_MAYBE_TYPE,
	AST2C_IDENTUSAGE_REFERENCE_BUILTIN_FN,
};
const char* ast2c_IdentUsage_to_string(ast2c_IdentUsage);
int ast2c_IdentUsage_from_string(const char*,ast2c_IdentUsage*);

enum ast2c_Endian {
	AST2C_ENDIAN_UNSPEC,
	AST2C_ENDIAN_BIG,
	AST2C_ENDIAN_LITTLE,
};
const char* ast2c_Endian_to_string(ast2c_Endian);
int ast2c_Endian_from_string(const char*,ast2c_Endian*);

enum ast2c_ConstantLevel {
	AST2C_CONSTANTLEVEL_UNKNOWN,
	AST2C_CONSTANTLEVEL_CONSTANT,
	AST2C_CONSTANTLEVEL_IMMUTABLE_VARIABLE,
	AST2C_CONSTANTLEVEL_VARIABLE,
};
const char* ast2c_ConstantLevel_to_string(ast2c_ConstantLevel);
int ast2c_ConstantLevel_from_string(const char*,ast2c_ConstantLevel*);

enum ast2c_BitAlignment {
	AST2C_BITALIGNMENT_BYTE_ALIGNED,
	AST2C_BITALIGNMENT_BIT_1,
	AST2C_BITALIGNMENT_BIT_2,
	AST2C_BITALIGNMENT_BIT_3,
	AST2C_BITALIGNMENT_BIT_4,
	AST2C_BITALIGNMENT_BIT_5,
	AST2C_BITALIGNMENT_BIT_6,
	AST2C_BITALIGNMENT_BIT_7,
	AST2C_BITALIGNMENT_NOT_TARGET,
	AST2C_BITALIGNMENT_NOT_DECIDABLE,
};
const char* ast2c_BitAlignment_to_string(ast2c_BitAlignment);
int ast2c_BitAlignment_from_string(const char*,ast2c_BitAlignment*);

enum ast2c_Follow {
	AST2C_FOLLOW_UNKNOWN,
	AST2C_FOLLOW_END,
	AST2C_FOLLOW_FIXED,
	AST2C_FOLLOW_CONSTANT,
	AST2C_FOLLOW_NORMAL,
};
const char* ast2c_Follow_to_string(ast2c_Follow);
int ast2c_Follow_from_string(const char*,ast2c_Follow*);

enum ast2c_IoMethod {
	AST2C_IOMETHOD_UNSPEC,
	AST2C_IOMETHOD_OUTPUT_PUT,
	AST2C_IOMETHOD_INPUT_PEEK,
	AST2C_IOMETHOD_INPUT_GET,
	AST2C_IOMETHOD_INPUT_BACKWARD,
	AST2C_IOMETHOD_INPUT_OFFSET,
	AST2C_IOMETHOD_INPUT_REMAIN,
	AST2C_IOMETHOD_INPUT_SUBRANGE,
	AST2C_IOMETHOD_CONFIG_ENDIAN_LITTLE,
	AST2C_IOMETHOD_CONFIG_ENDIAN_BIG,
	AST2C_IOMETHOD_CONFIG_ENDIAN_NATIVE,
	AST2C_IOMETHOD_CONFIG_BIT_ORDER_LSB,
	AST2C_IOMETHOD_CONFIG_BIT_ORDER_MSB,
};
const char* ast2c_IoMethod_to_string(ast2c_IoMethod);
int ast2c_IoMethod_from_string(const char*,ast2c_IoMethod*);

enum ast2c_SpecialLiteralKind {
	AST2C_SPECIALLITERALKIND_INPUT,
	AST2C_SPECIALLITERALKIND_OUTPUT,
	AST2C_SPECIALLITERALKIND_CONFIG,
};
const char* ast2c_SpecialLiteralKind_to_string(ast2c_SpecialLiteralKind);
int ast2c_SpecialLiteralKind_from_string(const char*,ast2c_SpecialLiteralKind*);

enum ast2c_OrderType {
	AST2C_ORDERTYPE_BYTE,
	AST2C_ORDERTYPE_BIT,
};
const char* ast2c_OrderType_to_string(ast2c_OrderType);
int ast2c_OrderType_from_string(const char*,ast2c_OrderType*);

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
	char** files;
	size_t files_size;
	ast2c_JsonAst* ast;
	ast2c_SrcError* error;
};

struct ast2c_TokenFile {
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

struct ast2c_BuiltinMember {
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
	ast2c_Expr* cond;
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
	ast2c_Expr* cond;
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

struct ast2c_BadExpr {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Type* expr_type;
	ast2c_ConstantLevel constant_level;
	char* content;
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
	ast2c_Expr* cond;
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
};

// returns 1 if succeed 0 if failed
int ast2c_Return_parse(ast2c_Ast* ,ast2c_Return*,ast2c_json_handlers*,void*);

struct ast2c_Break {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
};

// returns 1 if succeed 0 if failed
int ast2c_Break_parse(ast2c_Ast* ,ast2c_Break*,ast2c_json_handlers*,void*);

struct ast2c_Continue {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
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
	ast2c_Loc cast_loc;
};

// returns 1 if succeed 0 if failed
int ast2c_Function_parse(ast2c_Ast* ,ast2c_Function*,ast2c_json_handlers*,void*);

struct ast2c_BuiltinFunction {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_FunctionType* func_type;
};

// returns 1 if succeed 0 if failed
int ast2c_BuiltinFunction_parse(ast2c_Ast* ,ast2c_BuiltinFunction*,ast2c_json_handlers*,void*);

struct ast2c_BuiltinField {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_Type* field_type;
};

// returns 1 if succeed 0 if failed
int ast2c_BuiltinField_parse(ast2c_Ast* ,ast2c_BuiltinField*,ast2c_json_handlers*,void*);

struct ast2c_BuiltinObject {
	const ast2c_NodeType node_type;
	ast2c_Loc loc;
	ast2c_Member* belong;
	ast2c_StructType* belong_struct;
	ast2c_Ident* ident;
	ast2c_BuiltinMember** members;
	size_t members_size;
};

// returns 1 if succeed 0 if failed
int ast2c_BuiltinObject_parse(ast2c_Ast* ,ast2c_BuiltinObject*,ast2c_json_handlers*,void*);

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
