#pragma once
#ifndef __AST_H__
#define __AST_H__

#include <stdint.h>

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
	not,
	minus_sign,
};

enum BinaryOp {
	mul,
	div,
	mod,
	left_arithmetic_shift,
	right_arithmetic_shift,
	left_logical_shift,
	right_logical_shift,
	bit_and,
	add,
	sub,
	bit_or,
	bit_xor,
	equal,
	not_equal,
	less,
	less_or_eq,
	grater,
	grater_or_eq,
	logical_and,
	logical_or,
	cond_op_1,
	cond_op_2,
	range_exclusive,
	range_inclusive,
	assign,
	define_assign,
	const_assign,
	add_assign,
	sub_assign,
	mul_assign,
	div_assign,
	mod_assign,
	left_shift_assign,
	right_shift_assign,
	bit_and_assign,
	bit_or_assign,
	bit_xor_assign,
	comma,
};

enum IdentUsage {
	unknown,
	reference,
	define_variable,
	define_const,
	define_field,
	define_format,
	define_enum,
	define_fn,
	define_arg,
	reference_type,
};

enum Endian {
	unspec,
	big,
	little,
};

enum TokenTag {
	indent,
	space,
	line,
	punct,
	int_literal,
	bool_literal,
	str_literal,
	keyword,
	ident,
	comment,
	error,
	unknown,
};

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
};

struct Node {
	Loc loc;
};

struct Expr {
	Loc loc;
	Type* expr_type;
};

struct Literal {
	Loc loc;
	Type* expr_type;
};

struct Stmt {
	Loc loc;
};

struct Member {
	Loc loc;
};

struct Type {
	Loc loc;
};

struct Program {
	StructType* struct_type;
	Node** elements;
	Scope* global_scope;
	Loc loc;
};

struct Binary {
	Type* expr_type;
	BinaryOp op;
	Expr* left;
	Expr* right;
	Loc loc;
};

struct Unary {
	Type* expr_type;
	UnaryOp op;
	Expr* expr;
	Loc loc;
};

struct Cond {
	Type* expr_type;
	Expr* cond;
	Expr* then;
	Loc els_loc;
	Expr* els;
	Loc loc;
};

struct Ident {
	Type* expr_type;
	char* ident;
	IdentUsage usage;
	Node* base;
	Scope* scope;
	Loc loc;
};

struct Call {
	Type* expr_type;
	Expr* callee;
	Expr* raw_arguments;
	Expr** arguments;
	Loc end_loc;
	Loc loc;
};

struct If {
	Type* expr_type;
	Expr* cond;
	IndentScope* then;
	Node* els;
	Loc loc;
};

struct MemberAccess {
	Type* expr_type;
	Expr* target;
	char* member;
	Loc member_loc;
	Loc loc;
};

struct Paren {
	Type* expr_type;
	Expr* expr;
	Loc end_loc;
	Loc loc;
};

struct Index {
	Type* expr_type;
	Expr* expr;
	Expr* index;
	Loc end_loc;
	Loc loc;
};

struct Match {
	Type* expr_type;
	Expr* cond;
	Node** branch;
	Scope* scope;
	Loc loc;
};

struct Range {
	Type* expr_type;
	BinaryOp op;
	Expr* start;
	Expr* end;
	Loc loc;
};

struct TmpVar {
	Type* expr_type;
	uint64_t tmp_var;
	Loc loc;
};

struct BlockExpr {
	Type* expr_type;
	Node** calls;
	Expr* expr;
	Loc loc;
};

struct Import {
	Type* expr_type;
	char* path;
	Call* base;
	Program* import_desc;
	Loc loc;
};

struct IntLiteral {
	Type* expr_type;
	char* value;
	Loc loc;
};

struct BoolLiteral {
	Type* expr_type;
	int value;
	Loc loc;
};

struct StrLiteral {
	Type* expr_type;
	char* value;
	Loc loc;
};

struct Input {
	Type* expr_type;
	Loc loc;
};

struct Output {
	Type* expr_type;
	Loc loc;
};

struct Config {
	Type* expr_type;
	Loc loc;
};

struct Loop {
	Expr* init;
	Expr* cond;
	Expr* step;
	IndentScope* body;
	Loc loc;
};

struct IndentScope {
	Node** elements;
	Scope* scope;
	Loc loc;
};

struct MatchBranch {
	Expr* cond;
	Loc sym_loc;
	Node* then;
	Loc loc;
};

struct Return {
	Expr* expr;
	Loc loc;
};

struct Break {
	Loc loc;
};

struct Continue {
	Loc loc;
};

struct Assert {
	Binary* cond;
	Loc loc;
};

struct ImplicitYield {
	Expr* expr;
	Loc loc;
};

struct Field {
	Ident* ident;
	Loc colon_loc;
	Type* field_type;
	Expr* raw_arguments;
	Expr** arguments;
	Format* belong;
	Loc loc;
};

struct Format {
	int is_enum;
	Ident* ident;
	IndentScope* body;
	Format* belong;
	StructType* struct_type;
	Loc loc;
};

struct Function {
	Ident* ident;
	Field** parameters;
	Type* return_type;
	Format* belong;
	IndentScope* body;
	FunctionType* func_type;
	Loc loc;
};

struct IntType {
	uint64_t bit_size;
	Endian endian;
	int is_signed;
	Loc loc;
};

struct IdentType {
	Ident* ident;
	Format* base;
	Loc loc;
};

struct IntLiteralType {
	IntLiteral* base;
	Loc loc;
};

struct StrLiteralType {
	StrLiteral* base;
	Loc loc;
};

struct VoidType {
	Loc loc;
};

struct BoolType {
	Loc loc;
};

struct ArrayType {
	Loc end_loc;
	Type* base_type;
	Expr* length;
	Loc loc;
};

struct FunctionType {
	Type* return_type;
	Type** parameters;
	Loc loc;
};

struct StructType {
	Member** fields;
	Loc loc;
};

struct UnionType {
	StructType** fields;
	Expr* base;
	Loc loc;
};

struct Cast {
	Type* expr_type;
	Call* base;
	Expr* expr;
	Loc loc;
};

struct Comment {
	char* comment;
	Loc loc;
};

struct CommentGroup {
	Comment** comments;
	Loc loc;
};

struct Scope {
	Scope* prev;
	Scope* next;
	Scope* branch;
	Ident** ident;
};

#endif
