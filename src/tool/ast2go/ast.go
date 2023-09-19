package ast2go

import (
	"encoding/json"
	"errors"
	"fmt"
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
	Callee ExprNode
	Args   []ExprNode
	EndLoc Loc
}

type ParenNode struct {
	Loc    Loc
	Expr   ExprNode
	EndLoc Loc
}

type IfNode struct {
	Loc    Loc
	Cond   ExprNode
	Then   *IndentScopeNode
	Else   Node
	EndLoc Loc
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
	Loc  Loc
	Cond ExprNode
	Then Node
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
	Ident  *IdentNode
	Body   *IndentScopeNode
	Scope  *Scope
	Belong *FormatNode
}

type FieldNode struct {
	Loc    Loc
	Ident  *IdentNode
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
	Args   []*FieldNode
	Return TypeNode
	Body   *IndentScopeNode
	Scope  *Scope
}

type IntType struct {
	Loc     Loc
	BitSize int
	Raw     string
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
	Ident *IdentNode
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

type Program struct {
	Loc         Loc
	Elements    []Node
	GlobalScope *Scope
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

func (i *IndentScopeNode) node()     {}
func (i *IndentScopeNode) stmtNode() {}

func (i *Program) node() {}

func (a *AST) collectNodeLink(rawNodes []RawNode) error {
	for i, rawNode := range rawNodes {
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
		case "indent_scope":
			node = &IndentScopeNode{}
		case "program":
			node = &Program{}
		default:
			return errors.New("unknown node type: " + rawNode.NodeType)
		}
		a.Nodes[i] = node
	}
	return nil
}

func (a *AST) collectScopeLink(scope []jsonScope) {
	for i := range a.Scopes {
		a.Scopes[i] = &Scope{}
	}
}

func (a *AST) linkNode(rawNodes []RawNode) error {
	for i, rawNode := range rawNodes {
		body := rawNode.Body
		switch v := a.Nodes[i].(type) {
		case *IdentNode:
			var identTmp struct {
				Name  string  `json:"ident"`
				Base  *uint64 `json:"base"`
				Scope *uint64 `json:"scope"`
				Usage string  `json:"usage"`
			}
			if err := json.Unmarshal(body, &identTmp); err != nil {
				return err
			}
			v.Name = identTmp.Name
			if identTmp.Base != nil {
				v.Base = a.Nodes[*identTmp.Base].(*IdentNode)
			}
			if identTmp.Scope != nil {
				v.Scope = a.Scopes[*identTmp.Scope]
			}
			switch identTmp.Usage {
			case "unknown":
				v.Usage = IdentUsageUnknown
			case "reference":
				v.Usage = IdentUsageReference
			case "define_variable":
				v.Usage = IdentUsageDefineVariable
			case "define_const":
				v.Usage = IdentUsageDefineConst
			case "define_field":
				v.Usage = IdentUsageDefineField
			case "define_format":
				v.Usage = IdentUsageDefineFormat
			case "define_fn":
				v.Usage = IdentUsageDefineFn
			case "reference_type":
				v.Usage = IdentUsageReferenceType
			default:
				return errors.New("unknown ident usage: " + identTmp.Usage)
			}
		case *CallNode:
			var callTmp struct {
				Callee uint64   `json:"callee"`
				Args   []uint64 `json:"arguments"`
			}
			if err := json.Unmarshal(body, &callTmp); err != nil {
				return err
			}
			v.Callee = a.Nodes[callTmp.Callee].(ExprNode)
			if callTmp.Args != nil {
				v.Args = make([]ExprNode, len(callTmp.Args))
				for i, arg := range callTmp.Args {
					v.Args[i] = a.Nodes[arg].(ExprNode)
				}
			}
		case *Program:
			var programTmp struct {
				Elements []uint64 `json:"elements"`
				Scope    uint64   `json:"global_scope"`
			}
			if err := json.Unmarshal(body, &programTmp); err != nil {
				return err
			}
			v.Elements = make([]Node, len(programTmp.Elements))
			for i, element := range programTmp.Elements {
				v.Elements[i] = a.Nodes[element]
			}
			v.GlobalScope = a.Scopes[programTmp.Scope]
		case *ParenNode:
			var parenTmp struct {
				Expr uint64 `json:"expr"`
			}
			if err := json.Unmarshal(body, &parenTmp); err != nil {
				return err
			}
			v.Expr = a.Nodes[parenTmp.Expr].(ExprNode)
		case *IfNode:
			var ifTmp struct {
				Cond uint64  `json:"cond"`
				Then uint64  `json:"then"`
				Else *uint64 `json:"els"`
			}
			if err := json.Unmarshal(body, &ifTmp); err != nil {
				return err
			}
			v.Cond = a.Nodes[ifTmp.Cond].(ExprNode)
			v.Then = a.Nodes[ifTmp.Then].(*IndentScopeNode)
			if ifTmp.Else != nil {
				v.Else = a.Nodes[*ifTmp.Else]
			}
		case *UnaryNode:
			var unaryTmp struct {
				Op   string `json:"op"`
				Expr uint64 `json:"expr"`
			}
			if err := json.Unmarshal(body, &unaryTmp); err != nil {
				return err
			}
			v.Op = unaryTmp.Op
			v.Expr = a.Nodes[unaryTmp.Expr].(ExprNode)
		case *BinaryNode:
			var binaryTmp struct {
				Op    string `json:"op"`
				Left  uint64 `json:"left"`
				Right uint64 `json:"right"`
			}
			if err := json.Unmarshal(body, &binaryTmp); err != nil {
				return err
			}
			v.Op = binaryTmp.Op
			v.Left = a.Nodes[binaryTmp.Left].(ExprNode)
			v.Right = a.Nodes[binaryTmp.Right].(ExprNode)
		case *RangeNode:
			var rangeTmp struct {
				Op    string  `json:"op"`
				Begin *uint64 `json:"begin"`
				End   *uint64 `json:"end"`
			}
			if err := json.Unmarshal(body, &rangeTmp); err != nil {
				return err
			}
			v.Op = rangeTmp.Op
			if rangeTmp.Begin != nil {
				v.Begin = a.Nodes[*rangeTmp.Begin].(ExprNode)
			}
			if rangeTmp.End != nil {
				v.End = a.Nodes[*rangeTmp.End].(ExprNode)
			}
		case *MemberAccessNode:
			var memberAccessTmp struct {
				Target uint64 `json:"target"`
				Member string `json:"member"`
			}
			if err := json.Unmarshal(body, &memberAccessTmp); err != nil {
				return err
			}
			v.Target = a.Nodes[memberAccessTmp.Target].(ExprNode)
			v.Member = memberAccessTmp.Member
		case *CondNode:
			var condTmp struct {
				Cond uint64 `json:"cond"`
				Then uint64 `json:"then"`
				Else uint64 `json:"els"`
			}
			if err := json.Unmarshal(body, &condTmp); err != nil {
				return err
			}
			v.Cond = a.Nodes[condTmp.Cond].(ExprNode)
			v.Then = a.Nodes[condTmp.Then].(ExprNode)
			v.Else = a.Nodes[condTmp.Else].(ExprNode)
		case *IndexNode:
			var indexTmp struct {
				Target uint64 `json:"target"`
				Index  uint64 `json:"index"`
			}
			if err := json.Unmarshal(body, &indexTmp); err != nil {
				return err
			}
			v.Target = a.Nodes[indexTmp.Target].(ExprNode)
			v.Index = a.Nodes[indexTmp.Index].(ExprNode)
		case *MatchBranchNode:
			var matchBranchTmp struct {
				Target uint64 `json:"target"`
				Then   uint64 `json:"then"`
			}
			if err := json.Unmarshal(body, &matchBranchTmp); err != nil {
				return err
			}
			v.Cond = a.Nodes[matchBranchTmp.Target].(ExprNode)
			v.Then = a.Nodes[matchBranchTmp.Then]
		case *MatchNode:
			var matchTmp struct {
				Target uint64   `json:"target"`
				Branch []uint64 `json:"branch"`
			}
			if err := json.Unmarshal(body, &matchTmp); err != nil {
				return err
			}
			v.Target = a.Nodes[matchTmp.Target].(ExprNode)
			v.Branch = make([]*MatchBranchNode, len(matchTmp.Branch))
			for i, branch := range matchTmp.Branch {
				v.Branch[i] = a.Nodes[branch].(*MatchBranchNode)
			}
		case *IntLiteralNode:
			var intLiteralTmp struct {
				Value string `json:"value"`
			}
			if err := json.Unmarshal(body, &intLiteralTmp); err != nil {
				return err
			}
			v.Value = intLiteralTmp.Value
		case *StringLiteralNode:
			var stringLiteralTmp struct {
				Value string `json:"value"`
			}

			if err := json.Unmarshal(body, &stringLiteralTmp); err != nil {
				return err
			}
			v.Value = stringLiteralTmp.Value

		case *BoolLiteralNode:
			var boolLiteralTmp struct {
				Value bool `json:"value"`
			}
			if err := json.Unmarshal(body, &boolLiteralTmp); err != nil {
				return err
			}
			v.Value = boolLiteralTmp.Value
		case *InputNode:
		case *OutputNode:
		case *ConfigNode:
		case *FormatNode:
			var formatTmp struct {
				Ident  uint64  `json:"ident"`
				Body   uint64  `json:"body"`
				Scope  uint64  `json:"scope"`
				Belong *uint64 `json:"belong"`
			}
			if err := json.Unmarshal(body, &formatTmp); err != nil {
				return err
			}
			v.Ident = a.Nodes[formatTmp.Ident].(*IdentNode)
			v.Body = a.Nodes[formatTmp.Body].(*IndentScopeNode)
			v.Scope = a.Scopes[formatTmp.Scope]
			if formatTmp.Belong != nil {
				v.Belong = a.Nodes[*formatTmp.Belong].(*FormatNode)
			}
		case *FieldNode:
			var fieldTmp struct {
				Ident  uint64   `json:"ident"`
				Type   uint64   `json:"type"`
				Args   []uint64 `json:"args"`
				Scope  uint64   `json:"scope"`
				Belong *uint64  `json:"belong"`
			}
			if err := json.Unmarshal(body, &fieldTmp); err != nil {
				return err
			}
			v.Ident = a.Nodes[fieldTmp.Ident].(*IdentNode)
			v.Type = a.Nodes[fieldTmp.Type].(TypeNode)
			v.Args = make([]ExprNode, len(fieldTmp.Args))
			for i, arg := range fieldTmp.Args {
				v.Args[i] = a.Nodes[arg].(ExprNode)
			}
			v.Scope = a.Scopes[fieldTmp.Scope]
			if fieldTmp.Belong != nil {
				v.Belong = a.Nodes[*fieldTmp.Belong].(*FormatNode)
			}
		case *LoopNode:
			var loopTmp struct {
				Body uint64 `json:"body"`
			}
			if err := json.Unmarshal(body, &loopTmp); err != nil {
				return err
			}
			v.Body = a.Nodes[loopTmp.Body].(*IndentScopeNode)
		case *FunctionNode:
			var functionTmp struct {
				Name   string   `json:"name"`
				Args   []uint64 `json:"args"`
				Return uint64   `json:"return"`
				Body   uint64   `json:"body"`
				Scope  uint64   `json:"scope"`
			}
			if err := json.Unmarshal(body, &functionTmp); err != nil {
				return err
			}
			v.Name = functionTmp.Name
			v.Args = make([]*FieldNode, len(functionTmp.Args))
			for i, arg := range functionTmp.Args {
				v.Args[i] = a.Nodes[arg].(*FieldNode)
			}
			v.Return = a.Nodes[functionTmp.Return].(TypeNode)
			v.Body = a.Nodes[functionTmp.Body].(*IndentScopeNode)
			v.Scope = a.Scopes[functionTmp.Scope]
		case *IntType:
			var intTypeTmp struct {
				BitSize int    `json:"bit_size"`
				Raw     string `json:"raw"`
			}
			if err := json.Unmarshal(body, &intTypeTmp); err != nil {
				return err
			}
			v.BitSize = intTypeTmp.BitSize
			v.Raw = intTypeTmp.Raw
		case *IntLiteralType:
			var intLiteralTypeTmp struct {
				Base uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &intLiteralTypeTmp); err != nil {
				return err
			}
			v.Base = a.Nodes[intLiteralTypeTmp.Base].(*IntLiteralNode)
		case *StringLiteralType:
			var stringLiteralTypeTmp struct {
				Base uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &stringLiteralTypeTmp); err != nil {
				return err
			}
			v.Base = a.Nodes[stringLiteralTypeTmp.Base].(*StringLiteralNode)
		case *BoolType:
		case *IdentType:
			var identTypeTmp struct {
				Ident uint64 `json:"ident"`
				Base  uint64 `json:"base"`
			}
			if err := json.Unmarshal(body, &identTypeTmp); err != nil {
				return err
			}
			v.Ident = a.Nodes[identTypeTmp.Ident].(*IdentNode)
			v.Base = a.Nodes[identTypeTmp.Base].(*FormatNode)
		case *VoidType:
		case *ArrayType:
			var arrayTypeTmp struct {
				Base   uint64 `json:"base"`
				Length uint64 `json:"length"`
			}
			if err := json.Unmarshal(body, &arrayTypeTmp); err != nil {
				return err
			}
			v.Base = a.Nodes[arrayTypeTmp.Base].(TypeNode)
			v.Length = a.Nodes[arrayTypeTmp.Length].(ExprNode)
		case *IndentScopeNode:
			var indentScopeTmp struct {
				Elements []uint64 `json:"elements"`
				Scope    uint64   `json:"scope"`
			}
			if err := json.Unmarshal(body, &indentScopeTmp); err != nil {
				return err
			}
			v.Elements = make([]Node, len(indentScopeTmp.Elements))
			for i, element := range indentScopeTmp.Elements {
				v.Elements[i] = a.Nodes[element]
			}
			v.Scope = a.Scopes[indentScopeTmp.Scope]
		default:
			return errors.New("unknown node type: " + rawNode.NodeType)

		}
	}
	return nil
}

func (a *AST) UnmarshalJSON(s []byte) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("type error: %v", r)
		}
	}()
	jsonAST := jsonAST{}
	if err := json.Unmarshal(s, &jsonAST); err != nil {
		return err
	}
	a.NodeIndex = make(map[Node]uint64)
	a.ScopeIndex = make(map[*Scope]uint64)
	a.Nodes = make([]Node, len(jsonAST.RawNodes))
	a.Scopes = make([]*Scope, len(jsonAST.RawScopes))
	err = a.collectNodeLink(jsonAST.RawNodes)
	if err != nil {
		return err
	}
	a.collectScopeLink(jsonAST.RawScopes)
	if err = a.linkNode(jsonAST.RawNodes); err != nil {
		return err
	}
	return nil
}
