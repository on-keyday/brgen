#pragma once
#ifndef __AST_H__
#define __AST_H__

#include <stdint.h>
#include "json_stub.h"

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
	INDENT_SCOPE,
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
	UNION_TYPE,
	CAST,
	COMMENT,
	COMMENT_GROUP,
} NodeType;

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
typedef struct IndentScope IndentScope;
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
typedef struct UnionType UnionType;
typedef struct Cast Cast;
typedef struct Comment Comment;
typedef struct CommentGroup CommentGroup;
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
UnaryOp UnaryOp_from_string(const char*);
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
BinaryOp BinaryOp_from_string(const char*);
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
IdentUsage IdentUsage_from_string(const char*);
enum Endian {
	UNSPEC,
	BIG,
	LITTLE,
};

const char* Endian_to_string(Endian);
Endian Endian_from_string(const char*);
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
TokenTag TokenTag_from_string(const char*);
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
};

struct Type {
	const NodeType node_type;
	Loc loc;
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
	Expr* cond;
	IndentScope* then;
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
	Expr* cond;
	Node** branch;
	size_t branch_size;
	Scope* scope;
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
	Expr* init;
	Expr* cond;
	Expr* step;
	IndentScope* body;
};

struct IndentScope {
	const NodeType node_type;
	Loc loc;
	Node** elements;
	size_t elements_size;
	Scope* scope;
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
	Ident* ident;
	Loc colon_loc;
	Type* field_type;
	Expr* raw_arguments;
	Expr** arguments;
	size_t arguments_size;
	Format* belong;
};

struct Format {
	const NodeType node_type;
	Loc loc;
	int is_enum;
	Ident* ident;
	IndentScope* body;
	Format* belong;
	StructType* struct_type;
};

struct Function {
	const NodeType node_type;
	Loc loc;
	Ident* ident;
	Field** parameters;
	size_t parameters_size;
	Type* return_type;
	Format* belong;
	IndentScope* body;
	FunctionType* func_type;
};

struct IntType {
	const NodeType node_type;
	Loc loc;
	uint64_t bit_size;
	Endian endian;
	int is_signed;
};

struct IdentType {
	const NodeType node_type;
	Loc loc;
	Ident* ident;
	Format* base;
};

struct IntLiteralType {
	const NodeType node_type;
	Loc loc;
	IntLiteral* base;
};

struct StrLiteralType {
	const NodeType node_type;
	Loc loc;
	StrLiteral* base;
};

struct VoidType {
	const NodeType node_type;
	Loc loc;
};

struct BoolType {
	const NodeType node_type;
	Loc loc;
};

struct ArrayType {
	const NodeType node_type;
	Loc loc;
	Loc end_loc;
	Type* base_type;
	Expr* length;
};

struct FunctionType {
	const NodeType node_type;
	Loc loc;
	Type* return_type;
	Type** parameters;
	size_t parameters_size;
};

struct StructType {
	const NodeType node_type;
	Loc loc;
	Member** fields;
	size_t fields_size;
};

struct UnionType {
	const NodeType node_type;
	Loc loc;
	StructType** fields;
	size_t fields_size;
	Expr* base;
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
	RawNode** node;
	size_t node_size;
	RawScope** scope;
	size_t scope_size;
} Ast;

#endif
