using System;
using System.CodeDom.Compiler;
using System.Collections.Generic;
namespace ast2cs
{
	public interface Node
	{
		public Loc Loc { get; set; }
	}
	public interface Expr : Node
	{
		public Type? ExprType { get; set; }
	}
	public interface Literal : Expr
	{
	}
	public interface Stmt : Node
	{
	}
	public interface Member : Stmt
	{
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
	}
	public interface Type : Node
	{
		public bool IsExplicit { get; set; }
	}
	public class Program : Node
	{
		public Loc Loc { get; set; }
		public StructType? StructType { get; set; }
		public List<Node>? Elements { get; set; }
		public Scope? GlobalScope { get; set; }
	}
	public class Binary : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public BinaryOp Op { get; set; }
		public Expr? Left { get; set; }
		public Expr? Right { get; set; }
	}
	public class Unary : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public UnaryOp Op { get; set; }
		public Expr? Expr { get; set; }
	}
	public class Cond : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Expr? Cond_ { get; set; }
		public Expr? Then { get; set; }
		public Loc ElsLoc { get; set; }
		public Expr? Els { get; set; }
	}
	public class Ident : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public string Ident_ { get; set; }
		public IdentUsage Usage { get; set; }
		public Node? Base { get; set; }
		public Scope? Scope { get; set; }
	}
	public class Call : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Expr? Callee { get; set; }
		public Expr? RawArguments { get; set; }
		public List<Expr>? Arguments { get; set; }
		public Loc EndLoc { get; set; }
	}
	public class If : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Scope? CondScope { get; set; }
		public Expr? Cond { get; set; }
		public IndentBlock? Then { get; set; }
		public Node? Els { get; set; }
	}
	public class MemberAccess : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Expr? Target { get; set; }
		public string Member { get; set; }
		public Loc MemberLoc { get; set; }
		public Node? Base { get; set; }
	}
	public class Paren : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Expr? Expr { get; set; }
		public Loc EndLoc { get; set; }
	}
	public class Index : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Expr? Expr { get; set; }
		public Expr? Index_ { get; set; }
		public Loc EndLoc { get; set; }
	}
	public class Match : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Scope? CondScope { get; set; }
		public Expr? Cond { get; set; }
		public List<Node>? Branch { get; set; }
	}
	public class Range : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public BinaryOp Op { get; set; }
		public Expr? Start { get; set; }
		public Expr? End { get; set; }
	}
	public class TmpVar : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public ulong TmpVar_ { get; set; }
	}
	public class BlockExpr : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public List<Node>? Calls { get; set; }
		public Expr? Expr { get; set; }
	}
	public class Import : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public string Path { get; set; }
		public Call? Base { get; set; }
		public Program? ImportDesc { get; set; }
	}
	public class IntLiteral : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public string Value { get; set; }
	}
	public class BoolLiteral : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public bool Value { get; set; }
	}
	public class StrLiteral : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public string Value { get; set; }
	}
	public class Input : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
	}
	public class Output : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
	}
	public class Config : Literal
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
	}
	public class Loop : Stmt
	{
		public Loc Loc { get; set; }
		public Scope? CondScope { get; set; }
		public Expr? Init { get; set; }
		public Expr? Cond { get; set; }
		public Expr? Step { get; set; }
		public IndentBlock? Body { get; set; }
	}
	public class IndentBlock : Stmt
	{
		public Loc Loc { get; set; }
		public List<Node>? Elements { get; set; }
		public Scope? Scope { get; set; }
		public StructType? StructType { get; set; }
	}
	public class MatchBranch : Stmt
	{
		public Loc Loc { get; set; }
		public Expr? Cond { get; set; }
		public Loc SymLoc { get; set; }
		public Node? Then { get; set; }
	}
	public class Return : Stmt
	{
		public Loc Loc { get; set; }
		public Expr? Expr { get; set; }
	}
	public class Break : Stmt
	{
		public Loc Loc { get; set; }
	}
	public class Continue : Stmt
	{
		public Loc Loc { get; set; }
	}
	public class Assert : Stmt
	{
		public Loc Loc { get; set; }
		public Binary? Cond { get; set; }
	}
	public class ImplicitYield : Stmt
	{
		public Loc Loc { get; set; }
		public Expr? Expr { get; set; }
	}
	public class Field : Member
	{
		public Loc Loc { get; set; }
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
		public Loc ColonLoc { get; set; }
		public Type? FieldType { get; set; }
		public Expr? RawArguments { get; set; }
		public List<Expr>? Arguments { get; set; }
	}
	public class Format : Member
	{
		public Loc Loc { get; set; }
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
		public IndentBlock? Body { get; set; }
	}
	public class Function : Member
	{
		public Loc Loc { get; set; }
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
		public List<Field>? Parameters { get; set; }
		public Type? ReturnType { get; set; }
		public IndentBlock? Body { get; set; }
		public FunctionType? FuncType { get; set; }
		public bool IsCast { get; set; }
		public Loc CastLoc { get; set; }
	}
	public class IntType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public ulong BitSize { get; set; }
		public Endian Endian { get; set; }
		public bool IsSigned { get; set; }
	}
	public class IdentType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Ident? Ident { get; set; }
		public Member? Base { get; set; }
	}
	public class IntLiteralType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public IntLiteral? Base { get; set; }
	}
	public class StrLiteralType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public StrLiteral? Base { get; set; }
	}
	public class VoidType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
	}
	public class BoolType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
	}
	public class ArrayType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Loc EndLoc { get; set; }
		public Type? BaseType { get; set; }
		public Expr? Length { get; set; }
	}
	public class FunctionType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Type? ReturnType { get; set; }
		public List<Type>? Parameters { get; set; }
	}
	public class StructType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public List<Member>? Fields { get; set; }
	}
	public class StructUnionType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public List<StructType>? Fields { get; set; }
		public Expr? Base { get; set; }
		public List<Field>? UnionFields { get; set; }
	}
	public class Cast : Expr
	{
		public Loc Loc { get; set; }
		public Type? ExprType { get; set; }
		public Call? Base { get; set; }
		public Expr? Expr { get; set; }
	}
	public class Comment : Node
	{
		public Loc Loc { get; set; }
		public string Comment_ { get; set; }
	}
	public class CommentGroup : Node
	{
		public Loc Loc { get; set; }
		public List<Comment>? Comments { get; set; }
	}
	public class UnionType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Expr? Cond { get; set; }
		public List<UnionCandidate>? Candidates { get; set; }
		public StructUnionType? BaseType { get; set; }
	}
	public class UnionCandidate : Stmt
	{
		public Loc Loc { get; set; }
		public Expr? Cond { get; set; }
		public Member? Field { get; set; }
	}
	public class RangeType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Type? BaseType { get; set; }
		public Range? Range { get; set; }
	}
	public class Enum : Member
	{
		public Loc Loc { get; set; }
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
		public Scope? Scope { get; set; }
		public Loc ColonLoc { get; set; }
		public Type? BaseType { get; set; }
		public List<EnumMember>? Members { get; set; }
		public EnumType? EnumType { get; set; }
	}
	public class EnumMember : Member
	{
		public Loc Loc { get; set; }
		public Member? Belong { get; set; }
		public Ident? Ident { get; set; }
		public Expr? Expr { get; set; }
	}
	public class EnumType : Type
	{
		public Loc Loc { get; set; }
		public bool IsExplicit { get; set; }
		public Enum? Base { get; set; }
	}
	public enum UnaryOp
	{
		Not,
		MinusSign,
	}
	public enum BinaryOp
	{
		Mul,
		Div,
		Mod,
		LeftArithmeticShift,
		RightArithmeticShift,
		LeftLogicalShift,
		RightLogicalShift,
		BitAnd,
		Add,
		Sub,
		BitOr,
		BitXor,
		Equal,
		NotEqual,
		Less,
		LessOrEq,
		Grater,
		GraterOrEq,
		LogicalAnd,
		LogicalOr,
		CondOp1,
		CondOp2,
		RangeExclusive,
		RangeInclusive,
		Assign,
		DefineAssign,
		ConstAssign,
		AddAssign,
		SubAssign,
		MulAssign,
		DivAssign,
		ModAssign,
		LeftShiftAssign,
		RightShiftAssign,
		BitAndAssign,
		BitOrAssign,
		BitXorAssign,
		Comma,
	}
	public enum IdentUsage
	{
		Unknown,
		Reference,
		DefineVariable,
		DefineConst,
		DefineField,
		DefineFormat,
		DefineEnum,
		DefineEnumMember,
		DefineFn,
		DefineCastFn,
		DefineArg,
		ReferenceType,
	}
	public enum Endian
	{
		Unspec,
		Big,
		Little,
	}
	public enum TokenTag
	{
		Indent,
		Space,
		Line,
		Punct,
		IntLiteral,
		BoolLiteral,
		StrLiteral,
		Keyword,
		Ident,
		Comment,
		Error,
		Unknown,
	}
	public class Scope
	{
		public Scope? Prev { get; set; }
		public Scope? Next { get; set; }
		public Scope? Branch { get; set; }
		public List<Ident>? Ident { get; set; }
		public Node? Owner { get; set; }
	}
	public class Pos
	{
		public ulong Begin { get; set; }
		public ulong End { get; set; }
	}
	public class Loc
	{
		public Pos Pos { get; set; }
		public ulong File { get; set; }
		public ulong Line { get; set; }
		public ulong Col { get; set; }
	}
	public class Token
	{
		public TokenTag Tag { get; set; }
		public string Token_ { get; set; }
		public Loc Loc { get; set; }
	}
	public class SrcErrorEntry
	{
		public string Msg { get; set; }
		public string File { get; set; }
		public Loc Loc { get; set; }
		public string Src { get; set; }
		public bool Warn { get; set; }
	}
	public class SrcError
	{
		public List<SrcErrorEntry>? Errs { get; set; }
	}


}