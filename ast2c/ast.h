// Code generated by gen_ast2c.go. DO NOT EDIT.

#pragma once
#ifndef __AST_H__
#define __AST_H__

#include <stdint.h>
#include "json_stub.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NodeType {
	NODE,
	PROGRAM,
	EXPR,
	BINARY,
	UNARY,
	COND,
	IDENT,
	CALL,
	IF,
	MEMBER_ACCESS,
	PAREN,
	INDEX,
	MATCH,
	RANGE,
	TMP_VAR,
	BLOCK_EXPR,
	IMPORT,
	LITERAL,
	INT_LITERAL,
	BOOL_LITERAL,
	STR_LITERAL,
	INPUT,
	OUTPUT,
	CONFIG,
	STMT,
	LOOP,
	INDENT_BLOCK,
	MATCH_BRANCH,
	RETURN,
	BREAK,
	CONTINUE,
	ASSERT,
	IMPLICIT_YIELD,
	MEMBER,
	FIELD,
	FORMAT,
	FUNCTION,
	TYPE,
	INT_TYPE,
	IDENT_TYPE,
	INT_LITERAL_TYPE,
	STR_LITERAL_TYPE,
	VOID_TYPE,
	BOOL_TYPE,
	ARRAY_TYPE,
	FUNCTION_TYPE,
	STRUCT_TYPE,
	STRUCT_UNION_TYPE,
	CAST,
	COMMENT,
	COMMENT_GROUP,
	UNION_TYPE,
	UNION_CANDIDATE,
	RANGE_TYPE,
} NodeType;

int NodeType_from_string(const char*, NodeType*);
const char* NodeType_to_string(NodeType);

typedef struct Node Node;
typedef struct Expr Expr;
typedef struct Literal Literal;
typedef struct Stmt Stmt;
typedef struct Member Member;
typedef struct Type Type;
typedef struct Program Program;
typedef struct Binary Binary;
typedef struct Unary Unary;
typedef struct Cond Cond;
typedef struct Ident Ident;
typedef struct Call Call;
typedef struct If If;
typedef struct MemberAccess MemberAccess;
typedef struct Paren Paren;
typedef struct Index Index;
typedef struct Match Match;
typedef struct Range Range;
typedef struct TmpVar TmpVar;
typedef struct BlockExpr BlockExpr;
typedef struct Import Import;
typedef struct IntLiteral IntLiteral;
typedef struct BoolLiteral BoolLiteral;
typedef struct StrLiteral StrLiteral;
typedef struct Input Input;
typedef struct Output Output;
typedef struct Config Config;
typedef struct Loop Loop;
typedef struct IndentBlock IndentBlock;
typedef struct MatchBranch MatchBranch;
typedef struct Return Return;
typedef struct Break Break;
typedef struct Continue Continue;
typedef struct Assert Assert;
typedef struct ImplicitYield ImplicitYield;
typedef struct Field Field;
typedef struct Format Format;
typedef struct Function Function;
typedef struct IntType IntType;
typedef struct IdentType IdentType;
typedef struct IntLiteralType IntLiteralType;
typedef struct StrLiteralType StrLiteralType;
typedef struct VoidType VoidType;
typedef struct BoolType BoolType;
typedef struct ArrayType ArrayType;
typedef struct FunctionType FunctionType;
typedef struct StructType StructType;
typedef struct StructUnionType StructUnionType;
typedef struct Cast Cast;
typedef struct Comment Comment;
typedef struct CommentGroup CommentGroup;
typedef struct UnionType UnionType;
typedef struct UnionCandidate UnionCandidate;
typedef struct RangeType RangeType;
typedef enum UnaryOp UnaryOp;
typedef enum BinaryOp BinaryOp;
typedef enum IdentUsage IdentUsage;
typedef enum Endian Endian;
typedef enum TokenTag TokenTag;
typedef struct Scope Scope;
typedef struct Pos Pos;
typedef struct Loc Loc;
typedef struct Token Token;
typedef struct SrcErrorEntry SrcErrorEntry;
typedef struct SrcError SrcError;
enum UnaryOp {
	NOT,
	MINUS_SIGN,
};
const char* UnaryOp_to_string(UnaryOp);
int UnaryOp_from_string(const char*,UnaryOp*);

enum BinaryOp {
	MUL,
	DIV,
	MOD,
	LEFT_ARITHMETIC_SHIFT,
	RIGHT_ARITHMETIC_SHIFT,
	LEFT_LOGICAL_SHIFT,
	RIGHT_LOGICAL_SHIFT,
	BIT_AND,
	ADD,
	SUB,
	BIT_OR,
	BIT_XOR,
	EQUAL,
	NOT_EQUAL,
	LESS,
	LESS_OR_EQ,
	GRATER,
	GRATER_OR_EQ,
	LOGICAL_AND,
	LOGICAL_OR,
	COND_OP_1,
	COND_OP_2,
	RANGE_EXCLUSIVE,
	RANGE_INCLUSIVE,
	ASSIGN,
	DEFINE_ASSIGN,
	CONST_ASSIGN,
	ADD_ASSIGN,
	SUB_ASSIGN,
	MUL_ASSIGN,
	DIV_ASSIGN,
	MOD_ASSIGN,
	LEFT_SHIFT_ASSIGN,
	RIGHT_SHIFT_ASSIGN,
	BIT_AND_ASSIGN,
	BIT_OR_ASSIGN,
	BIT_XOR_ASSIGN,
	COMMA,
};
const char* BinaryOp_to_string(BinaryOp);
int BinaryOp_from_string(const char*,BinaryOp*);

enum IdentUsage {
	UNKNOWN,
	REFERENCE,
	DEFINE_VARIABLE,
	DEFINE_CONST,
	DEFINE_FIELD,
	DEFINE_FORMAT,
	DEFINE_ENUM,
	DEFINE_FN,
	DEFINE_ARG,
	REFERENCE_TYPE,
};
const char* IdentUsage_to_string(IdentUsage);
int IdentUsage_from_string(const char*,IdentUsage*);

enum Endian {
	UNSPEC,
	BIG,
	LITTLE,
};
const char* Endian_to_string(Endian);
int Endian_from_string(const char*,Endian*);

enum TokenTag {
	INDENT,
	SPACE,
	LINE,
	PUNCT,
	INT_LITERAL,
	BOOL_LITERAL,
	STR_LITERAL,
	KEYWORD,
	IDENT,
	COMMENT,
	ERROR,
	UNKNOWN,
};
const char* TokenTag_to_string(TokenTag);
int TokenTag_from_string(const char*,TokenTag*);

struct Pos {
	uint64_t begin;
	uint64_t end;
};

struct Loc {
	Pos pos;
	uint64_t file;
	uint64_t line;
	uint64_t col;
};

struct Token {
	TokenTag tag;
	char* token;
	Loc loc;
};

struct SrcErrorEntry {
	char* msg;
	char* file;
	Loc loc;
	char* src;
	int warn;
};

struct SrcError {
	SrcErrorEntry* errs;
	size_t errs_size;
};

struct Node {
	const NodeType node_type;
	Loc loc;
};

struct Expr {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
};

struct Literal {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
};

struct Stmt {
	const NodeType node_type;
	Loc loc;
};

struct Member {
	const NodeType node_type;
	Loc loc;
	Member* belong;
	Ident* ident;
};

struct Type {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
};

struct Program {
	const NodeType node_type;
	Loc loc;
	StructType* struct_type;
	Node** elements;
	size_t elements_size;
	Scope* global_scope;
};

struct Binary {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	BinaryOp op;
	Expr* left;
	Expr* right;
};

struct Unary {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	UnaryOp op;
	Expr* expr;
};

struct Cond {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Expr* cond;
	Expr* then;
	Loc els_loc;
	Expr* els;
};

struct Ident {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	char* ident;
	IdentUsage usage;
	Node* base;
	Scope* scope;
};

struct Call {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Expr* callee;
	Expr* raw_arguments;
	Expr** arguments;
	size_t arguments_size;
	Loc end_loc;
};

struct If {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Scope* cond_scope;
	Expr* cond;
	IndentBlock* then;
	Node* els;
};

struct MemberAccess {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Expr* target;
	char* member;
	Loc member_loc;
};

struct Paren {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Expr* expr;
	Loc end_loc;
};

struct Index {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Expr* expr;
	Expr* index;
	Loc end_loc;
};

struct Match {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Scope* cond_scope;
	Expr* cond;
	Node** branch;
	size_t branch_size;
};

struct Range {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	BinaryOp op;
	Expr* start;
	Expr* end;
};

struct TmpVar {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	uint64_t tmp_var;
};

struct BlockExpr {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Node** calls;
	size_t calls_size;
	Expr* expr;
};

struct Import {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	char* path;
	Call* base;
	Program* import_desc;
};

struct IntLiteral {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	char* value;
};

struct BoolLiteral {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	int value;
};

struct StrLiteral {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	char* value;
};

struct Input {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
};

struct Output {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
};

struct Config {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
};

struct Loop {
	const NodeType node_type;
	Loc loc;
	Scope* cond_scope;
	Expr* init;
	Expr* cond;
	Expr* step;
	IndentBlock* body;
};

struct IndentBlock {
	const NodeType node_type;
	Loc loc;
	Node** elements;
	size_t elements_size;
	Scope* scope;
	StructType* struct_type;
};

struct MatchBranch {
	const NodeType node_type;
	Loc loc;
	Expr* cond;
	Loc sym_loc;
	Node* then;
};

struct Return {
	const NodeType node_type;
	Loc loc;
	Expr* expr;
};

struct Break {
	const NodeType node_type;
	Loc loc;
};

struct Continue {
	const NodeType node_type;
	Loc loc;
};

struct Assert {
	const NodeType node_type;
	Loc loc;
	Binary* cond;
};

struct ImplicitYield {
	const NodeType node_type;
	Loc loc;
	Expr* expr;
};

struct Field {
	const NodeType node_type;
	Loc loc;
	Member* belong;
	Ident* ident;
	Loc colon_loc;
	Type* field_type;
	Expr* raw_arguments;
	Expr** arguments;
	size_t arguments_size;
};

struct Format {
	const NodeType node_type;
	Loc loc;
	Member* belong;
	Ident* ident;
	int is_enum;
	IndentBlock* body;
};

struct Function {
	const NodeType node_type;
	Loc loc;
	Member* belong;
	Ident* ident;
	Field** parameters;
	size_t parameters_size;
	Type* return_type;
	IndentBlock* body;
	FunctionType* func_type;
};

struct IntType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	uint64_t bit_size;
	Endian endian;
	int is_signed;
};

struct IdentType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Ident* ident;
	Format* base;
};

struct IntLiteralType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	IntLiteral* base;
};

struct StrLiteralType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	StrLiteral* base;
};

struct VoidType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
};

struct BoolType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
};

struct ArrayType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Loc end_loc;
	Type* base_type;
	Expr* length;
};

struct FunctionType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Type* return_type;
	Type** parameters;
	size_t parameters_size;
};

struct StructType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Member** fields;
	size_t fields_size;
};

struct StructUnionType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	StructType** fields;
	size_t fields_size;
	Expr* base;
	Field** union_fields;
	size_t union_fields_size;
};

struct Cast {
	const NodeType node_type;
	Loc loc;
	Type* expr_type;
	Call* base;
	Expr* expr;
};

struct Comment {
	const NodeType node_type;
	Loc loc;
	char* comment;
};

struct CommentGroup {
	const NodeType node_type;
	Loc loc;
	Comment** comments;
	size_t comments_size;
};

struct UnionType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Expr* cond;
	UnionCandidate** candidate;
	size_t candidate_size;
	StructUnionType* base_type;
};

struct UnionCandidate {
	const NodeType node_type;
	Loc loc;
	Expr* cond;
	Member* field;
};

struct RangeType {
	const NodeType node_type;
	Loc loc;
	int is_explicit;
	Type* base_type;
	Range* range;
};

struct Scope {
	const NodeType node_type;
	Scope* prev;
	Scope* next;
	Scope* branch;
	Ident** ident;
	size_t ident_size;
	int is_global;
};

typedef struct RawNode {
	const NodeType node_type;
	Loc loc;
	void* body;
} RawNode;

typedef struct RawScope {
	uint64_t prev;
	uint64_t next;
	uint64_t branch;
	uint64_t* ident;
	size_t ident_size;
	int is_global;
} RawScope;

typedef struct Ast {
	RawNode* node;
	size_t node_size;
	RawScope* scope;
	size_t scope_size;
} Ast;

#ifdef __cplusplus
}
#endif

#endif
