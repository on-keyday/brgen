package ast2go

import "encoding/json"

type Node interface {
	node()
	L() Loc
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

		}
		a.NodeIndex[node] = uint64(i)
		a.Nodes[i] = node
	}
	return nil
}
