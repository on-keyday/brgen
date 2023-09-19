package ast2go

import (
	"encoding/json"
	"errors"
)

type Node interface {
	node()
}

type Pos struct {
	Begin int `json:"begin"`
	End   int `json:"end"`
}

type Loc struct {
	Pos  Pos `json:"pos"`
	File int `json:"file"`
}

type NodeHdr struct {
	NodeType string `json:"node_type"`
	Loc      Loc    `json:"loc"`
}

type RawNode struct {
	NodeHdr
	Body json.RawMessage `json:"body"`
}

type Object interface {
	Ident() string
}

type Scope struct {
	/* in C++ code core/ast/node/scope.h
			std::weak_ptr<Scope> prev;
	        std::list<Object> objects;
	        std::shared_ptr<Scope> branch;
	        std::shared_ptr<Scope> next;
	*/
	Prev    *Scope
	Next    *Scope
	Objects []Object
	Branch  *Scope
}

type AST struct {
	/*
		in C++ code core/ast/json.h
		std::map<std::shared_ptr<Node>, size_t> node_index;
		std::map<std::shared_ptr<Scope>, size_t> scope_index;
		std::vector<std::shared_ptr<Node>> nodes;
		std::vector<std::shared_ptr<Scope>> scopes;
	*/
	NodeIndex  map[Node]uint64
	ScopeIndex map[*Scope]uint64
	Nodes      []Node
	Scopes     []*Scope
}

type jsonScope struct {
	Prev    uint64 `json:"prev"`
	Next    uint64 `json:"next"`
	Objects []uint64
	Branch  uint64 `json:"branch"`
}

type jsonAST struct {
	RawNodes  []RawNode   `json:"node"`
	RawScopes []jsonScope `json:"scope"`
}

// NodeBody represents the body of a node.
// body is a union in C++ code core/ast/node/*.h which derived from brgen::ast::Node

type ExprNode interface {
	Node
	exprNode()
}

type LiteralNode interface {
	ExprNode
	literalNode()
}

type TypeNode interface {
	Node
	typeNode()
}

type StmtNode interface {
	Node
	stmtNode()
}

type IndentScopeNode struct {
	Loc      Loc
	Elements []Node
	Scope    *Scope
}

type IdentUsage int

const (
	IdentUsageUnknown IdentUsage = iota
	IdentUsageReference
	IdentUsageDefineVariable
	IdentUsageDefineConst
	IdentUsageDefineField
	IdentUsageDefineFormat
	IdentUsageDefineFn
	IdentUsageReferenceType
)

func (i IdentUsage) String() string {
	switch i {
	case IdentUsageUnknown:
		return "unknown"
	case IdentUsageReference:
		return "reference"
	case IdentUsageDefineVariable:
		return "define_variable"
	case IdentUsageDefineConst:
		return "define_const"
	case IdentUsageDefineField:
		return "define_field"
	case IdentUsageDefineFormat:
		return "define_format"
	case IdentUsageDefineFn:
		return "define_fn"
	case IdentUsageReferenceType:
		return "reference_type"
	default:
		return "unknown"
	}
}

type IdentNode struct {
	Loc   Loc
	Name  string
	Base  *IdentNode
	Scope *Scope
	Usage IdentUsage
}

type CallNode struct {
	Loc    Loc
	Target ExprNode
	Args   []ExprNode
	EndLoc Loc
}

type ParenNode struct {
	Loc    Loc
	Expr   ExprNode
	EndLoc Loc
}

type IfNode struct {
	Loc      Loc
	Cond     ExprNode
	ThenBody *IndentScopeNode
	ElseBody Node
	EndLoc   Loc
}

type UnaryNode struct {
	Loc  Loc
	Op   string
	Expr ExprNode
}

type BinaryNode struct {
	Loc   Loc
	Op    string
	Left  ExprNode
	Right ExprNode
}

type RangeNode struct {
	Loc   Loc
	Op    string
	Begin ExprNode
	End   ExprNode
}

type MemberAccessNode struct {
	Loc    Loc
	Target ExprNode
	Member string
}

type CondNode struct {
	Loc  Loc
	Cond ExprNode
	Then ExprNode
	Else ExprNode
}

type IndexNode struct {
	Loc    Loc
	Target ExprNode
	Index  ExprNode
	EndLoc Loc
}

type MatchBranchNode struct {
	Loc    Loc
	Target ExprNode
	Body   Node
}

type MatchNode struct {
	Loc    Loc
	Target ExprNode
	Branch []*MatchBranchNode
}

type IntLiteralNode struct {
	Loc   Loc
	Value string
}

type StringLiteralNode struct {
	Loc   Loc
	Value string
}

type BoolLiteralNode struct {
	Loc   Loc
	Value bool
}

type InputNode struct {
	Loc Loc
}

type OutputNode struct {
	Loc Loc
}

type ConfigNode struct {
	Loc Loc
}

type FormatNode struct {
	Loc    Loc
	Name   string
	Body   *IndentScopeNode
	Scope  *Scope
	Belong *FormatNode
}

type FieldNode struct {
	Loc    Loc
	Name   string
	Type   TypeNode
	Args   []ExprNode
	Scope  *Scope
	Belong *FormatNode
}

type LoopNode struct {
	Loc  Loc
	Body *IndentScopeNode
}

type FunctionNode struct {
	Loc    Loc
	Name   string
	Args   []FieldNode
	Return TypeNode
	Body   *IndentScopeNode
	Scope  *Scope
}

type IntType struct {
	Loc     Loc
	BitSize int
}

type IntLiteralType struct {
	Loc  Loc
	Base *IntLiteralNode
}

type StringLiteralType struct {
	Loc  Loc
	Base *StringLiteralNode
}

type BoolType struct {
	Loc Loc
}

type IdentType struct {
	Loc   Loc
	Name  string
	Scope *Scope
	Base  *FormatNode
}

type VoidType struct {
	Loc Loc
}

type ArrayType struct {
	Loc    Loc
	Base   TypeNode
	Length ExprNode
}

func (i *IdentNode) node()     {}
func (i *IdentNode) exprNode() {}

func (i *CallNode) node()     {}
func (i *CallNode) exprNode() {}

func (i *ParenNode) node()     {}
func (i *ParenNode) exprNode() {}

func (i *IfNode) node()     {}
func (i *IfNode) exprNode() {}

func (i *UnaryNode) node()     {}
func (i *UnaryNode) exprNode() {}

func (i *BinaryNode) node()     {}
func (i *BinaryNode) exprNode() {}

func (i *RangeNode) node()     {}
func (i *RangeNode) exprNode() {}

func (i *MemberAccessNode) node()     {}
func (i *MemberAccessNode) exprNode() {}

func (i *CondNode) node()     {}
func (i *CondNode) exprNode() {}

func (i *IndexNode) node()     {}
func (i *IndexNode) exprNode() {}

func (i *MatchBranchNode) node()     {}
func (i *MatchBranchNode) stmtNode() {}

func (i *MatchNode) node()     {}
func (i *MatchNode) exprNode() {}

func (i *IntLiteralNode) node()        {}
func (i *IntLiteralNode) exprNode()    {}
func (i *IntLiteralNode) literalNode() {}

func (i *StringLiteralNode) node()        {}
func (i *StringLiteralNode) exprNode()    {}
func (i *StringLiteralNode) literalNode() {}

func (i *BoolLiteralNode) node()        {}
func (i *BoolLiteralNode) exprNode()    {}
func (i *BoolLiteralNode) literalNode() {}

func (i *InputNode) node()        {}
func (i *InputNode) exprNode()    {}
func (i *InputNode) literalNode() {}

func (i *OutputNode) node()        {}
func (i *OutputNode) exprNode()    {}
func (i *OutputNode) literalNode() {}

func (i *ConfigNode) node()        {}
func (i *ConfigNode) exprNode()    {}
func (i *ConfigNode) literalNode() {}

func (i *FormatNode) node()     {}
func (i *FormatNode) stmtNode() {}

func (i *FieldNode) node()     {}
func (i *FieldNode) stmtNode() {}

func (i *LoopNode) node()     {}
func (i *LoopNode) stmtNode() {}

func (i *FunctionNode) node()     {}
func (i *FunctionNode) stmtNode() {}

func (i *IntType) node()     {}
func (i *IntType) typeNode() {}

func (i *IntLiteralType) node()     {}
func (i *IntLiteralType) typeNode() {}

func (i *StringLiteralType) node()     {}
func (i *StringLiteralType) typeNode() {}

func (i *BoolType) node()     {}
func (i *BoolType) typeNode() {}

func (i *IdentType) node()     {}
func (i *IdentType) typeNode() {}

func (i *VoidType) node()     {}
func (i *VoidType) typeNode() {}

func (i *ArrayType) node()     {}
func (i *ArrayType) typeNode() {}

func (a *AST) collectLink() {

}

func (a *AST) UnmarshalJSON(s []byte) error {
	jsonAST := jsonAST{}
	if err := json.Unmarshal(s, &jsonAST); err != nil {
		return err
	}
	a.NodeIndex = make(map[Node]uint64)
	a.ScopeIndex = make(map[*Scope]uint64)
	a.Nodes = make([]Node, len(jsonAST.RawNodes))
	a.Scopes = make([]*Scope, len(jsonAST.RawScopes))
	for i, rawNode := range jsonAST.RawNodes {
		var node Node
		switch rawNode.NodeType {
		case "ident":
			node = &IdentNode{}
		case "call":
			node = &CallNode{}
		case "paren":
			node = &ParenNode{}
		case "if":
			node = &IfNode{}
		case "unary":
			node = &UnaryNode{}
		case "binary":
			node = &BinaryNode{}
		case "range":
			node = &RangeNode{}
		case "member_access":
			node = &MemberAccessNode{}
		case "cond":
			node = &CondNode{}
		case "index":
			node = &IndexNode{}
		case "match_branch":
			node = &MatchBranchNode{}
		case "match":
			node = &MatchNode{}
		case "int_literal":
			node = &IntLiteralNode{}
		case "string_literal":
			node = &StringLiteralNode{}
		case "bool_literal":
			node = &BoolLiteralNode{}
		case "input":
			node = &InputNode{}
		case "output":
			node = &OutputNode{}
		case "config":
			node = &ConfigNode{}
		case "format":
			node = &FormatNode{}
		case "field":
			node = &FieldNode{}
		case "loop":
			node = &LoopNode{}
		case "function":
			node = &FunctionNode{}
		case "int_type":
			node = &IntType{}
		case "int_literal_type":
			node = &IntLiteralType{}
		case "string_literal_type":
			node = &StringLiteralType{}
		case "bool_type":
			node = &BoolType{}
		case "ident_type":
			node = &IdentType{}
		case "void_type":
			node = &VoidType{}
		case "array_type":
			node = &ArrayType{}
		default:
			return errors.New("unknown node type: " + rawNode.NodeType)
		}
		a.NodeIndex[node] = uint64(i)
		a.Nodes[i] = node
	}
	return nil
}
